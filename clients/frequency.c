#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "i2c.h"
#include "timespec.h"
#include "i2c_registers.h"
#include "adc_calc.h"
#include "rtc_data.h"

#define SAMPLES 64
#define CHANNELS 2

void get_i2c(int fd, struct i2c_registers_type *i2c_registers) {
  uint8_t set_page[2];

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE1;
  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  read_i2c(fd, i2c_registers, sizeof(struct i2c_registers_type));
  unlock_i2c(fd);

  if(i2c_registers->page_offset != I2C_REGISTER_PAGE1) {
    printf("got wrong page offset: %u != %u\n", i2c_registers->page_offset, I2C_REGISTER_PAGE1);
    exit(1);
  }

  if(i2c_registers->version != I2C_REGISTER_VERSION) { // TODO: handle compatability
    printf("got unexpected version: %u != %u\n", i2c_registers->version, I2C_REGISTER_VERSION);
    exit(1);
  }
}

float get_ppm() {
  FILE *freq;
  float ppm;

  freq = fopen("/var/run/tcxo-freq", "r");
  if(!freq) {
    perror("open /var/run/tcxo-freq");
    exit(1);
  }
  fscanf(freq, "%f", &ppm);
  fclose(freq);

  return ppm;
}

float get_sys_ppm() {
  FILE *freq;
  float ppm;

  freq = fopen("/var/run/tcxo", "r");
  if(!freq) {
    perror("open /var/run/tcxo");
    exit(1);
  }
  fscanf(freq, "%f", &ppm);
  fclose(freq);

  return ppm;
}

uint32_t sleep_calc(const struct i2c_registers_type *i2c_registers) {
  uint32_t sleepms;

  sleepms = i2c_registers->milliseconds_now - i2c_registers->milliseconds_irq_primary;
  sleepms = 1500 - sleepms; // aim for 500ms after primary

  if(sleepms < 10) {
    sleepms = 10;
  } else if(sleepms > 1000) {
    sleepms = 1000;
  }

  return sleepms;
}

void update_offsets(uint64_t *offsets, uint8_t *counts, uint32_t *previous_cycles, double ns_per_cycle, uint32_t cycles) {
  if(*counts == 0) {
    *previous_cycles = cycles;
    offsets[0] = 0;
    (*counts)++;
    return;
  }

  if(*previous_cycles == cycles) { // counter hasn't updated
    return;
  }

  if(*counts == SAMPLES) {
    for(uint8_t i = 0; i < SAMPLES; i++) {
      offsets[i] = offsets[i+1];
    }
  } else {
    (*counts)++;
  }

  uint32_t delta = cycles - *previous_cycles;
  uint64_t ns = delta * ns_per_cycle;

  offsets[(*counts)-1] = offsets[(*counts)-2] + ns;
  *previous_cycles = cycles;
}

float ppm(const uint64_t *offsets, uint8_t counts, uint32_t *seconds_ret, uint8_t maxcount) {
  uint64_t difference;
  uint32_t seconds;

  if(counts <= 1) {
    if(seconds_ret != NULL)
      *seconds_ret = 0;
    return 0;
  }

  if(maxcount > counts) {
    maxcount = 0;
  } else {
    maxcount = counts-maxcount;
  }

  difference = offsets[counts-1] - offsets[maxcount]; 
  seconds = (difference+5e8) / 1e9;
  if(seconds_ret != NULL)
    *seconds_ret = seconds;
  return (difference - seconds * 1e9) / (seconds*1000.0);
}

int main() {
  int fd;
  struct i2c_registers_type i2c_registers;
  uint64_t offsets[CHANNELS][SAMPLES];
  uint32_t previous_cycles[CHANNELS];
  uint8_t counts[CHANNELS];

  fd = open_i2c(I2C_ADDR); 

  for(uint8_t i = 0; i < CHANNELS; i++) {
    counts[i] = 0;
    offsets[i][0] = 0;
    previous_cycles[i] = 0;
  }

  //printf("ts sleep ch0 ch1 tcxo ppm0_16s ppm0 sys s0 ppm1 s1 off0 off1\n");
  printf("ts sleep ch0 ch1 ch0_freq ch1_freq\n");
  while(1) {
    uint32_t sleepms;
    float tcxo_ppm, sys_ppm;
    double ns_per_cycle;
    float ch_ppm[CHANNELS], ch0_16s;
    uint32_t ch_s[CHANNELS];

    get_i2c(fd, &i2c_registers);
    tcxo_ppm = get_ppm();
    sys_ppm = get_sys_ppm();

    ns_per_cycle = 1/0.048 * (1+tcxo_ppm/1000000.0);

    sleepms = sleep_calc(&i2c_registers);

    printf("%lu %u %u %u %u %u %u %u %u %u\n", time(NULL), sleepms,
            i2c_registers.tim2_at_cap[0], i2c_registers.tim2_at_cap[1], i2c_registers.tim2_at_cap[2], i2c_registers.tim2_at_cap[3],
            (i2c_registers.tim2_at_cap[0] - previous_cycles[0]), (i2c_registers.tim2_at_cap[1] - previous_cycles[1]),
            (i2c_registers.tim2_at_cap[2] - previous_cycles[2]), (i2c_registers.tim2_at_cap[3] - previous_cycles[3])
          );
    fflush(stdout);
    for(uint8_t i = 0; i < 4; i++) {
      previous_cycles[i] = i2c_registers.tim2_at_cap[i];
    }

    usleep(sleepms*1000);
  }
}

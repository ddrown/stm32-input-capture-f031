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
#include "i2c_registers.h"

// 3.3V per count of 4096 in nV
#define NV_PER_COUNT 805664

void get_i2c(int fd, struct i2c_registers_type *i2c_registers, struct i2c_registers_type_page2 *i2c_registers_page2) {
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

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE2;
  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  read_i2c(fd, i2c_registers_page2, sizeof(struct i2c_registers_type_page2));
  unlock_i2c(fd);

  if(i2c_registers_page2->page_offset != I2C_REGISTER_PAGE2) {
    printf("got wrong page offset: %u != %u\n", i2c_registers_page2->page_offset, I2C_REGISTER_PAGE2);
    exit(1);
  }
}

uint64_t avg_64(uint64_t *values, uint8_t length) {
  uint64_t sum = 0;
  for(uint8_t i = 0; i < length; i++) {
    sum += values[i];
  }
  return sum / length;
}

int32_t avg_i(int32_t *values, uint8_t length) {
  int64_t sum = 0;
  for(uint8_t i = 0; i < length; i++) {
    sum += values[i];
  }
  return sum / length;
}

#define AVERAGE_SAMPLES 60

static int32_t temps[AVERAGE_SAMPLES];
static uint64_t vccs[AVERAGE_SAMPLES];
static int8_t adc_index = -1;

int main() {
  int fd;
  struct i2c_registers_type i2c_registers;
  struct i2c_registers_type_page2 i2c_registers_page2;

  fd = open_i2c(I2C_ADDR);

  while(1) {
    uint32_t sleepms;
    get_i2c(fd, &i2c_registers, &i2c_registers_page2);

    if(adc_index < (AVERAGE_SAMPLES-1)) {
      adc_index++;
    } else {
      for(uint8_t i = 0; i < (AVERAGE_SAMPLES-1); i++) {
	temps[i] = temps[i+1];
	vccs[i] = vccs[i+1];
      }
    }

    sleepms = i2c_registers.milliseconds_now - i2c_registers_page2.last_adc_ms;

    uint32_t vref_expected_nv = i2c_registers_page2.vrefint_cal * NV_PER_COUNT;
    uint32_t vref_actual_nv = i2c_registers_page2.internal_vref * NV_PER_COUNT;
    uint64_t vcc_nv = (uint64_t)((vref_expected_nv+500)/1000)*3300000000/((vref_actual_nv+500)/1000);
//    printf("expected = %u actual = %u\n", vref_expected_nv, vref_actual_nv);

    vccs[adc_index] = vcc_nv;
    vcc_nv = avg_64(vccs, adc_index+1);

    uint32_t nv_per_count = vcc_nv / 4096;

//    printf("nv per count = %u (%u)\n", nv_per_count, NV_PER_COUNT);

    uint32_t temp_nv = i2c_registers_page2.internal_temp * nv_per_count;
    uint32_t nv_30C = i2c_registers_page2.ts_cal1 * NV_PER_COUNT;
    uint32_t nv_110C = i2c_registers_page2.ts_cal2 * NV_PER_COUNT;

    int32_t uv_per_C = (((int32_t)nv_110C - (int32_t)nv_30C) / (110-30) + 500)/1000;
    int32_t temp_mC = ((int32_t)temp_nv - (int32_t)nv_30C) / uv_per_C + 30000;
    int32_t temp_mF = temp_mC * 9/5+32000;
    temps[adc_index] = temp_mF;
    temp_mF = avg_i(temps, adc_index+1);

// a = -6.09603, b = 0.109139, c = -0.000384409, d = 0.480387
// ppm = tcxo_a + tcxo_b * (F - tcxo_d) + tcxo_c * pow(F - tcxo_d, 2)

    uint32_t temp_uF = temp_mF*1000;
    int64_t ppt_a64 = 109139 * (int64_t)(temp_uF - 480387) / 1000000;
    int64_t ppt_b64 = ((int64_t)(temp_uF + 384))*(temp_uF - 480387) / -2601395909;
    int64_t ppt_64 = -6096030 + ppt_a64 + ppt_b64;
    int32_t ppb_32 = (ppt_64 + 500) / 1000;

    float temp_F = temp_mF/1000.0;
    float ppm_a = 0.109139 * (temp_F - 0.480387);
    float ppm_b = -0.000384409 * pow(temp_F - 0.480387, 2);
    float ppm = -6.09603 + ppm_a + ppm_b;

    printf("ppm %.6f a %.6f b %.6f\n", ppm, ppm_a, ppm_b);
    printf("ppb   %6d a %6lld b %6lld\n", ppb_32, ppt_a64, ppt_b64);

//    printf("t = %u 30C = %u 110C = %u\n", temp_nv, nv_30C, nv_110C);
//    printf("%d uV/C\n", uv_per_C);
//    printf("%d mC %d mF\n", temp_mC, temp_mF);
    printf("vcc = %llu, temp %d mF, freq %d ppb\n", vcc_nv/1000, temp_mF, ppb_32);

//    printf("delay %u\n", sleepms);

    fflush(stdout);

    sleepms = 1000+(50-sleepms);

    usleep(1000*sleepms);
  }
}

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#include "i2c.h"
#include "timespec.h"
#include "i2c_registers.h"
#include "adc_calc.h"
#include "vref_calc.h"
#include "aging.h"
#include "tempcomp.h"

#define SUMMARIZE_INTERVAL 64
struct per_second_stats {
  time_t when;
  uint32_t irq_to_i2c_ms;
  uint32_t status;
  uint32_t sleep_ms;
  double added_offset_ns[INPUT_CHANNELS];
  double tempcomp;
  float aging;
  double main_freq_32s;
  double main_freq_64s;
  float temp_f;
  float vref;
  float vbat;
} stats[SUMMARIZE_INTERVAL];
uint8_t stats_i = 0;

// current code assumptions: main channel never stops
struct icap_config {
  uint8_t verbose;
  uint8_t main_channel;
  uint8_t main_channel_hz;
} config;

#define AVERAGING_CYCLES 65
#define PPM_INVALID -1000000.0
#define AIM_AFTER_MS 5
#define TCXO_TEMPCOMP_TEMPFILE "/run/.tcxo"
#define TCXO_TEMPCOMP_FILE "/run/tcxo"
#define TCXO_FREQ_TEMPFILE "/run/.tcxo-freq"
#define TCXO_FREQ_FILE "/run/tcxo-freq"

// status_flags bitfields
#define STATUS_BAD_CH1        0b1
#define STATUS_BAD_CH2       0b10
#define STATUS_BAD_CH3      0b100
#define STATUS_BAD_CH4     0b1000
// BAD_CHANNEL_STATUS takes 0-indexed channels
#define BAD_CHANNEL_STATUS(x) (1 << (x))

static uint16_t wrap_add(int16_t a, int16_t b, uint16_t modulus) {
  a = a + b;
  if(a < 0) {
    a += modulus;
  }
  return a;
}

static uint16_t wrap_sub(int16_t a, int16_t b, uint16_t modulus) {
  return wrap_add(a, -1 * b, modulus);
}

static void print_ppm(float ppm) {
  if(ppm < 500 && ppm > -500) {
    printf("%1.3f ", ppm);
  } else {
    printf("- ");
  }
}

static void write_freqfile(float ppm, const char *tempfile, const char *finalfile) {
  FILE *tcxo;

  if(ppm > 500 || ppm < -500) {
    return;
  }

  tcxo = fopen(tempfile,"w");
  if(tcxo == NULL) {
    perror("fopen");
    printf("was trying to open file %s\n", tempfile);
    exit(1);
  }
  fprintf(tcxo, "%1.3f\n", ppm);
  fclose(tcxo);
  rename(tempfile, finalfile);
}

static void write_tcxo_ppm(float ppm) {
  write_freqfile(ppm, TCXO_FREQ_TEMPFILE, TCXO_FREQ_FILE);
}

static void write_tempcomp_ppm(float ppm) {
  write_freqfile(ppm, TCXO_TEMPCOMP_TEMPFILE, TCXO_TEMPCOMP_FILE);
}

// modifies cycles, first_cycle, last_cycle
static void store_added_offset(double added_offset_ns, struct timespec *cycles, uint16_t *first_cycle, uint16_t *last_cycle) {
  struct timespec *previous_cycle = NULL;
  uint16_t this_cycle_i;

  this_cycle_i = (*last_cycle + 1) % AVERAGING_CYCLES;
  if(*first_cycle != *last_cycle) {
    previous_cycle = &cycles[*last_cycle];
    if(*first_cycle == this_cycle_i) { // we've wrapped around, move the first pointer forward
      *first_cycle = (*first_cycle + 1) % AVERAGING_CYCLES;
    }
  }

  cycles[this_cycle_i].tv_sec = 0;
  cycles[this_cycle_i].tv_nsec = added_offset_ns;
  if(previous_cycle != NULL) {
    add_timespecs(&cycles[this_cycle_i], previous_cycle);
  }

  *last_cycle = this_cycle_i;
}

static float calc_ppm_2(struct timespec *end, struct timespec *start, uint16_t seconds) {
  double diff_s;
  struct timespec diff;
  diff.tv_sec = end->tv_sec;
  diff.tv_nsec = end->tv_nsec;

  sub_timespecs(&diff, start);
  diff_s = diff.tv_sec + diff.tv_nsec / 1000000000.0;

  float ppm = diff_s * 1000000.0 / seconds;
  return ppm;
}

static float calc_ppm(int16_t number_points, uint16_t last_cycle_index, uint16_t seconds, struct timespec *cycles) {
  float ppm = PPM_INVALID; // default: invalid PPM

  if(number_points >= seconds) {
    uint16_t start_index = wrap_sub(last_cycle_index, seconds, AVERAGING_CYCLES);
    ppm = calc_ppm_2(&cycles[last_cycle_index], &cycles[start_index], seconds);
  }

  return ppm;
}

static uint32_t calculate_sleep_ms(uint32_t milliseconds_now, uint32_t milliseconds_irq) {
  uint32_t sleep_ms = 1000 + AIM_AFTER_MS - (milliseconds_now - milliseconds_irq);
  if(sleep_ms > 1000+AIM_AFTER_MS) {
    sleep_ms = 1000+AIM_AFTER_MS;
  } else if(sleep_ms < 1) {
    sleep_ms = 1;
  }
  return sleep_ms;
}

// modifies added_offset_ns, status_flags
static int add_cycles(uint32_t *status_flags, double *added_offset_ns, const struct i2c_registers_type *i2c_registers) {
  static uint32_t previous_cycles[INPUT_CHANNELS] = {0,0,0,0};
  static uint8_t previous_count[INPUT_CHANNELS] = {0,0,0,0};
  static uint32_t primary_ms_last = 0;
  int retval = 1;

  for(uint8_t i = 0; i < INPUT_CHANNELS; i++) {
    int32_t diff;
    
    if(i == config.main_channel-1) {
      uint32_t milliseconds = i2c_registers->milliseconds_irq_primary - primary_ms_last;
      if(milliseconds < 2000 && milliseconds > 900) {
        diff = i2c_registers->tim2_at_cap[i] - previous_cycles[i] - EXPECTED_FREQ;
        added_offset_ns[i] = diff * 1000000000.0 / EXPECTED_FREQ;
      } else {
        added_offset_ns[i] = 0;
        retval = 0;
	*status_flags |= BAD_CHANNEL_STATUS(i);
      }
      primary_ms_last = i2c_registers->milliseconds_irq_primary;
    } else {
      uint8_t diff_count = i2c_registers->ch_count[i] - previous_count[i];

      if(diff_count == 1 || diff_count == 2) { // treating count as # of seconds, assume 1s/2s=ok, other values=invalid
	diff = i2c_registers->tim2_at_cap[i] - previous_cycles[i] - (EXPECTED_FREQ * diff_count);
	added_offset_ns[i] = diff * 1000000000.0 / (EXPECTED_FREQ * diff_count);
      } else {
	added_offset_ns[i] = 0;
	*status_flags |= BAD_CHANNEL_STATUS(i);
      }
    }

    previous_cycles[i] = i2c_registers->tim2_at_cap[i];
    previous_count[i] = i2c_registers->ch_count[i];
  }

  return retval;
}

static void before_loop() {
  if(config.verbose) {
    printf("ts delay status sleepms 1ppm 2ppm 3ppm 4ppm tempcomp 32s_ppm 64s_ppm ");
    printf("int-temp vref vbat aging");
    printf("\n");
  }
}

static void log_loop(struct per_second_stats *cur_stats) {
  if(config.verbose) {
    printf("%lu %2u %3o %4u ",
       cur_stats->when,
       cur_stats->irq_to_i2c_ms,
       cur_stats->status,
       cur_stats->sleep_ms
       );
    for(uint8_t i = 0; i < INPUT_CHANNELS; i++) {
      printf("%.3f ", cur_stats->added_offset_ns[i]/1000.0);
    }

    printf(" %3.1f ",cur_stats->tempcomp);

    print_ppm(cur_stats->main_freq_32s);
    print_ppm(cur_stats->main_freq_64s);

    printf("%.4f ", cur_stats->temp_f);
    printf("%.5f ", cur_stats->vref);
    printf("%.5f ", cur_stats->vbat);

    printf("%.3f ", cur_stats->aging);

    printf("\n");
    fflush(stdout);
  }
}

static void print_data_u32(uint32_t *data, uint32_t count) {
  uint32_t min, max, sum;
  if(count == 0) {
    printf("- ");
    return;
  }
  min = data[0];
  max = data[0];
  sum = 0;
  for(uint32_t i = 0; i < count; i++) {
    if(min > data[i])
      min = data[i];
    if(max < data[i])
      max = data[i];
    sum += data[i];
  }
  printf("%u/%.1f/%u ", min, sum/(float)count, max);
}

static void print_data_d(double *data, uint32_t count, int precision) {
  double min, max, sum;
  if(count == 0) {
    printf("- ");
    return;
  }
  min = data[0];
  max = data[0];
  sum = 0;
  for(uint32_t i = 0; i < count; i++) {
    if(min > data[i])
      min = data[i];
    if(max < data[i])
      max = data[i];
    sum += data[i];
  }
  printf("%.*f/%.*f/%.*f ", precision, min, precision, sum/count, precision, max);
}

static void summarize() {
  uint32_t stat_data[SUMMARIZE_INTERVAL];
  double stat_data_d[SUMMARIZE_INTERVAL];
  uint32_t count;

  printf("%lu %lu ", stats[0].when, stats[SUMMARIZE_INTERVAL-1].when);
  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data[i] = stats[i].irq_to_i2c_ms;
  }
  print_data_u32(stat_data, SUMMARIZE_INTERVAL);

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data[i] = stats[i].sleep_ms;
  }
  print_data_u32(stat_data, SUMMARIZE_INTERVAL);

  for(uint32_t chan = 0; chan < INPUT_CHANNELS; chan++) {
    count = 0;
    for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
      if(!(stats[i].status & BAD_CHANNEL_STATUS(chan))) {
        stat_data_d[count] = stats[i].added_offset_ns[chan]/1000.0;
        count++;
      }
    }
    print_data_d(stat_data_d, count, 3);
  }

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data_d[i] = stats[i].tempcomp / 1000.0;
  }
  print_data_d(stat_data_d, SUMMARIZE_INTERVAL, 3);

  count = 0;
  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    if(stats[i].main_freq_32s < 500 && stats[i].main_freq_32s > -500) {
      stat_data_d[count] = stats[i].main_freq_32s;
      count++;
    }
  }
  print_data_d(stat_data_d, count, 3);

  count = 0;
  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    if(stats[i].main_freq_64s < 500 && stats[i].main_freq_64s > -500) {
      stat_data_d[count] = stats[i].main_freq_64s;
      count++;
    }
  }
  print_data_d(stat_data_d, count, 3);

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data_d[i] = stats[i].temp_f;
  }
  print_data_d(stat_data_d, SUMMARIZE_INTERVAL, 4);

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data_d[i] = stats[i].vref;
  }
  print_data_d(stat_data_d, SUMMARIZE_INTERVAL, 5);

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data_d[i] = stats[i].vbat;
  }
  print_data_d(stat_data_d, SUMMARIZE_INTERVAL, 5);

  for(uint32_t i = 0; i < SUMMARIZE_INTERVAL; i++) {
    stat_data_d[i] = stats[i].aging;
  }
  print_data_d(stat_data_d, SUMMARIZE_INTERVAL, 3);

  printf("\n");
  fflush(stdout);
}

static void increment_stats_i() {
  stats_i++;
  if(stats_i >= SUMMARIZE_INTERVAL) {
    summarize();
    stats_i = 0;
  }
}

static void poll_i2c(int fd) {
  struct timespec offsets[AVERAGING_CYCLES];
  uint16_t first_cycle_index = 0, last_cycle_index = 0;
  uint32_t last_timestamp = 0;
  struct i2c_registers_type i2c_registers;
  struct i2c_registers_type_page2 i2c_registers_page2;
  struct i2c_registers_type_page3 i2c_registers_page3;
  struct tempcomp_data tempcomp_data;

  memset(offsets, '\0', sizeof(offsets));

  get_i2c_page3(fd, &i2c_registers_page3, &tempcomp_data);

  before_loop();
  while(1) {
    int16_t number_points;
    struct per_second_stats *cur_stats;

    cur_stats = &stats[stats_i];
    memset(cur_stats, '\0', sizeof(struct per_second_stats));
    cur_stats->when = time(NULL);

    get_i2c_structs(fd, &i2c_registers, &i2c_registers_page2);
    add_adc_data(&i2c_registers, &i2c_registers_page2);

    cur_stats->temp_f = last_temp()*9/5.0+32.0;
    cur_stats->vref = last_vref();
    cur_stats->vbat = last_vbat();

    // was there no new data?
    if(i2c_registers.milliseconds_irq_primary == last_timestamp) {
      cur_stats->status |= BAD_CHANNEL_STATUS(config.main_channel-1);
      printf("no new data\n");
      fflush(stdout);
      usleep(995000);
      increment_stats_i();
      continue;
    }
    cur_stats->irq_to_i2c_ms = i2c_registers.milliseconds_now - i2c_registers.milliseconds_irq_primary;
    last_timestamp = i2c_registers.milliseconds_irq_primary;

    // aim for 5ms after the event
    cur_stats->sleep_ms = calculate_sleep_ms(i2c_registers.milliseconds_now, i2c_registers.milliseconds_irq_primary);

    if(!add_cycles(&cur_stats->status, cur_stats->added_offset_ns, &i2c_registers)) {
      printf("first cycle, sleeping %d ms\n", cur_stats->sleep_ms);
      fflush(stdout);
      usleep(cur_stats->sleep_ms * 1000);
      increment_stats_i();
      continue;
    }

    cur_stats->tempcomp = tempcomp(&tempcomp_data);
    cur_stats->aging = calc_tcxo_aging();
    for(uint8_t i = 0; i < INPUT_CHANNELS; i++) {
      if(!(cur_stats->status & BAD_CHANNEL_STATUS(i))) {
	// adjust all channels by the expected tempcomp and aging
        cur_stats->added_offset_ns[i] += cur_stats->tempcomp + (cur_stats->aging*1000);
      }
    }
    store_added_offset(cur_stats->added_offset_ns[config.main_channel-1], offsets, &first_cycle_index, &last_cycle_index);

    number_points = wrap_sub(last_cycle_index, first_cycle_index, AVERAGING_CYCLES);

    cur_stats->main_freq_32s = calc_ppm(number_points, last_cycle_index, 32, offsets);
    cur_stats->main_freq_64s = calc_ppm(number_points, last_cycle_index, AVERAGING_CYCLES-1, offsets);
    write_tempcomp_ppm(cur_stats->main_freq_64s);
    write_tcxo_ppm(cur_stats->tempcomp / 1000.0);
    log_loop(cur_stats);

    increment_stats_i();
    usleep(cur_stats->sleep_ms * 1000);
  }
}

void help() {
  fprintf(stderr,"input-capture-i2c -c [primary channel] -z [primary channel hz]\n");
}

int main(int argc, char **argv) {
  int c;
  uint8_t set_page[2];
  uint8_t set_main_channel[3];
  int fd;

  // defaults
  config.verbose = 0;
  config.main_channel = 1;
  config.main_channel_hz = 50;

  while((c = getopt(argc, argv, "c:z:hv")) != -1) {
    switch(c) {
      case 'c':
        config.main_channel = atoi(optarg);
        break;
      case 'z':
        config.main_channel_hz = atoi(optarg);
        break;
      case 'h':
        help();
        exit(0);
        break;
      case 'v':
        config.verbose = 1;
        break;
      case '?':
        if(optopt == 'c' || optopt == 'z') {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
	  exit(1);
        }
        if(isprint(optopt)) {
          fprintf(stderr, "Unknown option -%c\n", optopt);
          exit(1);
        }
        fprintf(stderr, "Unknown option character 0x%x.\n", optopt);
        exit(1);
        break;
      default:
        fprintf(stderr, "getopt is being wierd (0x%x), aborting\n", c);
        exit(1);
        break;
    }
  }

  if(config.main_channel_hz > 255 || config.main_channel_hz < 1) {
    printf("invalid value for HZ: %d\n", config.main_channel_hz);
    exit(1);
  }
  if(config.main_channel > 4 || config.main_channel < 1) {
    printf("invalid value for channel: %d\n", config.main_channel);
    exit(1);
  }

  fd = open_i2c(I2C_ADDR); 

  // primary channel setting is on page1
  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE1;

  // set the primary channel # and HZ
  set_main_channel[0] = I2C_REGISTER_PRIMARY_CHANNEL;
  set_main_channel[1] = config.main_channel-1; // channel 1 = 0
  set_main_channel[2] = config.main_channel_hz;

  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  write_i2c(fd, set_main_channel, sizeof(set_main_channel));
  unlock_i2c(fd);

  read_tcxo_aging();

  poll_i2c(fd);
}

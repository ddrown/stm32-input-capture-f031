#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#include "i2c.h"
#include "i2c_registers.h"
#include "rtc_data.h"
#include "aging.h"

float lse_calibration_to_ppm(uint16_t calib) {
  return (calib & CALIBRATION_ADDCLK) ? 488.281 : 0.0 - 0.953674 * (calib & CALIBRATION_SUBCLK_MASK);
}

uint16_t ppm_to_lse_calibration(float ppm) {
  uint16_t calib = 0;
  uint16_t subclock;

  if(ppm > 0) {
    calib = calib | CALIBRATION_ADDCLK;
    ppm -= 488.281;
  }

  subclock = ppm / -0.953674;

  return calib | (subclock & CALIBRATION_SUBCLK_MASK);
}

static int compare_d(const void *a_p, const void *b_p) {
  double *a = (double *)a_p;
  double *b = (double *)b_p;

  if(*a < *b) {
    return -1;
  } else if(*a > *b) {
    return 1;
  } else {
    return 0;
  }
}

// note: changes the order of values
static double median_d(double *values, int length) {
  qsort(values, length, sizeof(double), compare_d);

  return values[length/2];
}

#define POLLS 5
#define POLL_SLEEP_US 10541
static double poll_median(int fd) {
  struct timeval rtc[POLLS];
  struct i2c_registers_type_page4 page4[POLLS];
  double local_ts[POLLS], rtc_ts[POLLS], diff[POLLS];

  for(uint8_t i = 0; i < POLLS; i++) {
    get_rtc(fd, &rtc[i], &page4[i]);
    rtc_ts[i] = rtc_to_double(&page4[i],NULL);
    local_ts[i] = rtc[i].tv_sec + rtc[i].tv_usec / 1000000.0;
    diff[i] = local_ts[i]-rtc_ts[i]; // positive: rtc slow, negative: rtc fast
    printf("sample %d value %.3f\n", i, diff[i]);
    usleep(POLL_SLEEP_US); 
  }

  return median_d(diff, POLLS);
}

static void get(int fd) {
  struct timeval rtc;
  struct i2c_registers_type_page4 page4;
  double rtc_ts, median;
  struct tm now;
  float ppm;

  get_rtc(fd, &rtc, &page4);

  rtc_ts = rtc_to_double(&page4,&now);

  median = poll_median(fd);

  printf("raw: ss_d = %u ss = %u dt = %u y = %u\n", page4.subsecond_div, page4.subseconds, page4.datetime, page4.year);
  printf("timedate: %u:%u:%u %u/%u/%u\n", 
      now.tm_hour, now.tm_min, now.tm_sec,
      now.tm_mon+1, now.tm_mday, now.tm_year+1900
      );
  ppm = lse_calibration_to_ppm(page4.lse_calibration);
  printf("calibrate = %u (%.3f ppm) set = %u\n", page4.lse_calibration, ppm, page4.set_rtc);
  printf("LSE/TIM14: milli=%u tim2=%u tim14=%u\n", page4.LSE_millis_irq, page4.LSE_tim2_irq, page4.LSE_tim14_cap);
  printf("time:%.3f offset:%.3f %s\n", rtc_ts, median, (median < 0) ? "fast" : "slow");
}

static void set(int fd) {
  struct i2c_registers_type_page4 page4;
  struct tm now;
  time_t now_ts = time(NULL);
  uint8_t set_page[2];
  uint8_t set_page4_datetime[5], set_page4_year[2], set_page4_apply[2];

  gmtime_r(&now_ts, &now);
  printf("set: %u:%u:%u %u/%u/%u\n", 
      now.tm_hour, now.tm_min, now.tm_sec,
      now.tm_mon+1, now.tm_mday, now.tm_year+1900
      );
  tm_to_rtc(&page4, &now);
  
  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE4;

  set_page4_datetime[0] = 4;
  memcpy(set_page4_datetime+1, &page4.datetime, sizeof(page4.datetime));

  set_page4_year[0] = 10;
  set_page4_year[1] = page4.year;

  set_page4_apply[0] = 11;
  set_page4_apply[1] = SET_RTC_DATETIME;

  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  write_i2c(fd, set_page4_datetime, sizeof(set_page4_datetime));
  write_i2c(fd, set_page4_year, sizeof(set_page4_year));
  write_i2c(fd, set_page4_apply, sizeof(set_page4_apply));
  unlock_i2c(fd);
}

static void setsubsecond(int fd, uint32_t addclk, uint32_t subclk) {
  struct i2c_registers_type_page4 page4;
  uint8_t set_page[2];
  uint8_t set_page4_subsecond[3], set_page4_apply[2];

  page4.subseconds = (addclk ? SUBSECOND_ADD1S : 0) | (subclk & SUBSECOND_SUBCLK_MASK);

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE4;

  set_page4_subsecond[0] = 2;
  memcpy(set_page4_subsecond+1, &page4.subseconds, sizeof(page4.subseconds));

  set_page4_apply[0] = 11;
  set_page4_apply[1] = SET_RTC_SUBSECOND;

  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  write_i2c(fd, set_page4_subsecond, sizeof(set_page4_subsecond));
  write_i2c(fd, set_page4_apply, sizeof(set_page4_apply));
  unlock_i2c(fd);
}

static void sync_rtc(int fd) {
  double median;
  FILE *log;

  log = fopen("/var/lib/pihatrtc/last_sync", "w");
  if(!log) {
    printf("failed to open /var/lib/pihatrtc/last_sync\n");
    exit(1);
  }

  median = poll_median(fd);

  fprintf(log,"start %ld\n", time(NULL));
  printf("median %.4f\n", median);
  fprintf(log,"median %.4f\n", median);
  while(fabs(median) > 1) {
    if(median > 0) {
      // rtc 1s+ slow
      setsubsecond(fd, 1, 0);
      median -= 1;
      printf("set +1s\n");
      fprintf(log,"set +1s\n");
    } else {
      // rtc 1s+ fast
      setsubsecond(fd, 0, 1024);
      median += 1;
      printf("set -1s\n");
      fprintf(log,"set -1s\n");
    }
    usleep(1200000); // allow setsubsecond to finish
  }
  if(median > 0.0009) { 
    // rtc slow
    setsubsecond(fd, 1, 1024*(1-median));
    printf("set +1s -%.0f counts\n", 1024*(1-median));
    fprintf(log,"set +1s -%.0f counts\n", 1024*(1-median));
  } else if(median < -0.0009) {
    // rtc fast
    setsubsecond(fd, 0, -1024*median);
    printf("set -%.0f counts\n", -1024*median);
    fprintf(log,"set -%.0f counts\n", -1024*median);
  } else {
    printf("no change\n");
    fprintf(log,"no change\n");
  }
}

static void set_system(int fd) {
  double median, rtc_ts, local_ts_before, local_ts_after;
  struct timeval rtc, jumptime, adjust_time;
  struct i2c_registers_type_page4 page4;
  struct tm now;

  get_rtc(fd, &rtc, &page4);

  rtc_ts = rtc_to_double(&page4,&now);

  local_ts_before = rtc.tv_sec + rtc.tv_usec / 1000000.0;
  gettimeofday(&adjust_time, NULL);
  local_ts_after = adjust_time.tv_sec + adjust_time.tv_usec / 1000000.0;
  rtc_ts += (local_ts_after-local_ts_before); // handle the milliseconds spent speaking i2c

  jumptime.tv_sec = rtc_ts;
  jumptime.tv_usec = (uint64_t)(rtc_ts * 1000000.0) % 1000000;

  printf("set time: %lu.%06lu\n", jumptime.tv_sec, jumptime.tv_usec);
  printf("was: %lu.%06lu\n", rtc.tv_sec, rtc.tv_usec);

  if(settimeofday(&jumptime, NULL) != 0) {
    perror("settimeofday");
    exit(1);
  }

  median = poll_median(fd);

  printf("median %.6f\n", median);
}

static void _setcalibration(int fd, uint16_t calib) {
  uint8_t set_page[2];
  uint8_t set_page4_calib[3], set_page4_apply[2];

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE4;

  set_page4_calib[0] = 8;
  memcpy(set_page4_calib+1, &calib, sizeof(calib));

  set_page4_apply[0] = 11;
  set_page4_apply[1] = SET_RTC_CALIBRATION;

  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  write_i2c(fd, set_page4_calib, sizeof(set_page4_calib));
  write_i2c(fd, set_page4_apply, sizeof(set_page4_apply));
  unlock_i2c(fd);
}

static void setcalibration(int fd, uint32_t addclk, uint32_t subclk) {
  struct i2c_registers_type_page4 page4;

  page4.lse_calibration = (addclk ? CALIBRATION_ADDCLK : 0) | (subclk & CALIBRATION_SUBCLK_MASK);

  _setcalibration(fd, page4.lse_calibration);

  printf("set to %u (%.3f ppm)\n", page4.lse_calibration, lse_calibration_to_ppm(page4.lse_calibration));
}

float read_tcxo_ppm() {
  FILE *f;
  float ppm;

  f = fopen("/run/tcxo-freq","r");
  if(f == NULL) {
    perror("fopen /run/tcxo-freq");
    exit(1);
  }
  fscanf(f, "%f", &ppm);
  fclose(f);

  return ppm;
}

void compare(int fd) {
  struct timeval rtc;
  struct i2c_registers_type_page4 page4, last_page4;
  float ppm_remainder = 0.0;

  memset(&last_page4, '\0', sizeof(last_page4));

  read_tcxo_aging();

  printf("LSE vs TCXO\n");
  printf("time offset ms tim2 tim14 tim2_vs_tim14 ppm tcxo lse_set lse_remainder aging\n");
  while(1) {
    double local_ts, rtc_ts, diff;

    get_rtc(fd, &rtc, &page4);

    rtc_ts = rtc_to_double(&page4,NULL);
    local_ts = rtc.tv_sec + rtc.tv_usec / 1000000.0;
    diff = local_ts-rtc_ts; // positive: rtc slow, negative: rtc fast

    // if last_page4 is set
    if(last_page4.LSE_millis_irq != 0 || last_page4.LSE_tim2_irq != 0) {
      uint32_t milli_diff, tim2_diff, s;
      uint16_t tim14_diff, expected_tim14_diff, calib;
      int32_t tim2_adjustment;
      float ppm, tcxo_ppm;

      milli_diff = page4.LSE_millis_irq - last_page4.LSE_millis_irq;
      tim2_diff = page4.LSE_tim2_irq - last_page4.LSE_tim2_irq;
      tim14_diff = page4.LSE_tim14_cap - last_page4.LSE_tim14_cap;

      // calculate what tim2 would have been if it was at capture time instead of irq time
      // (would be wrong for absolute offset, but it is correct for frequency measurement)
      expected_tim14_diff = (uint16_t)tim2_diff + last_page4.LSE_tim14_cap;
      tim2_adjustment = (int32_t)page4.LSE_tim14_cap - (int32_t)expected_tim14_diff;
      if(tim2_adjustment > 32767) {
        tim2_adjustment = tim2_adjustment - 65536; // assume adjustment range is +32767 .. -32768
      } else if(tim2_adjustment < -32768) {
        tim2_adjustment = tim2_adjustment + 65536; // assume adjustment range is +32767 .. -32768
      }

      s = ((milli_diff+500) / 1000); // round up at 0.5s
      ppm = (tim2_diff + tim2_adjustment)/(float)s;
      ppm = (ppm - EXPECTED_FREQ) / (float)(EXPECTED_FREQ / 1000000.0);
      tcxo_ppm = read_tcxo_ppm() + calc_tcxo_aging();
      ppm += tcxo_ppm;
      calib = ppm_to_lse_calibration(ppm + ppm_remainder);
      ppm_remainder += ppm - lse_calibration_to_ppm(calib);

      // if we actually have timestamp data
      if(milli_diff != 0) { 
        printf("%lu %.3f %u %u %u %d %.3f %.3f %.3f %.3f %.3f\n", time(NULL), diff, milli_diff, tim2_diff, tim14_diff, tim2_adjustment, ppm, tcxo_ppm, lse_calibration_to_ppm(calib), ppm_remainder, calc_tcxo_aging());
        _setcalibration(fd, calib);

        memcpy(&last_page4, &page4, sizeof(page4));
      } else {
        printf("%lu %.3f %u %u %u %d - %.3f - - %.3f no data\n", time(NULL), diff, milli_diff, tim2_diff, tim14_diff, tim2_adjustment, tcxo_ppm, calc_tcxo_aging());
      }
    } else {
      printf("%lu %.3f - - - - - - %.3f\n", time(NULL), diff, calc_tcxo_aging());
      memcpy(&last_page4, &page4, sizeof(page4));
    }
    fflush(stdout);
    sleep(30);
  }
}

void offset(int fd) {
  struct timeval rtc, timers, before_timers;
  struct i2c_registers_type_page4 page4;
  struct i2c_registers_type_page5 page5;
  uint32_t subsecond;
  int32_t rtt;
  double rtc_ts, tim2_rtc_ts, local_ts, offset_s;
  struct tm now;

  while(1) {
    get_rtc(fd, &rtc, &page4);
    get_timers(fd, &before_timers, &timers, &page5);

    rtc_ts = rtc_to_double(&page4,&now);

    subsecond = page5.cur_tim2 - page4.tim2_rtc_second;

    tim2_rtc_ts = (uint32_t)rtc_ts;
    tim2_rtc_ts += subsecond / 48000000.0;
    if(rtc_ts > tim2_rtc_ts)
      tim2_rtc_ts += 1;

    local_ts = timers.tv_sec + timers.tv_usec / 1000000.0;

    offset_s = local_ts - tim2_rtc_ts;

    rtt = timers.tv_usec - before_timers.tv_usec;
    if(before_timers.tv_sec < timers.tv_sec)
      rtt += 1000000; 

    printf("%.6f %.6f %.6f %d\n", local_ts, offset_s, tim2_rtc_ts-rtc_ts, rtt);

    sleep(1);
  }
}

int main(int argc, char **argv) {
  int fd;

  // boot modifier: wait for the i2c bus to show up
  if(argc > 1 && strcmp(argv[1], "boot") == 0) {
    for(uint8_t i = 0; i < 40; i++) {
      if(access("/dev/i2c-1", F_OK) == 0) {
	break;
      }
      if(i == 0) {
        printf("/dev/i2c-1 does not exist, sleeping up to 2s\n");
      }
      usleep(50000);
    }
    argc--;
    argv = argv + 1;
  }

  fd = open_i2c(I2C_ADDR); 

  if(argc == 1) {
    printf("commands: get, set, setsystem, setsubsecond, setcalibration, sync, compare\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    get(fd);
    return 0;
  }

  if(strcmp(argv[1], "set") == 0) {
    set(fd);
    return 0;
  }

  if(strcmp(argv[1], "setsubsecond") == 0) {
    if(argc != 4) {
      printf("setsubsecond arguments: [1=+1s,0=nothing added] [0..32767 clks removed (0.977ms each)]\n");
      exit(1);
    }
    setsubsecond(fd, atoi(argv[2]), atoi(argv[3]));
    return 0;
  }

  if(strcmp(argv[1], "setcalibration") == 0) {
    if(argc != 4) {
      printf("setcalibration arguments: [1=+488ppm,0=no clks added/32s] [0..511 clks removed/32s (0.953ppm each)]\n");
      exit(1);
    }
    setcalibration(fd, atoi(argv[2]), atoi(argv[3]));
    return 0;
  }

  if(strcmp(argv[1], "sync") == 0) {
    sync_rtc(fd);
    return 0;
  }

  if(strcmp(argv[1], "setsystem") == 0) {
    set_system(fd);
    return 0;
  }

  if(strcmp(argv[1], "compare") == 0) {
    compare(fd);
    return 0;
  }

  if(strcmp(argv[1], "offset") == 0) {
    offset(fd);
    return 0;
  }

  printf("unknown command\n");
  exit(1);
}

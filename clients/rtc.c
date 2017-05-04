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

float lse_calibration_to_ppm(uint16_t calib) {
  return (calib & CALIBRATION_ADDCLK) ? 488.281 : 0.0 - 0.953 * (calib & CALIBRATION_SUBCLK_MASK);
}

static void get(int fd) {
  struct timeval rtc;
  struct i2c_registers_type_page4 page4;
  double local_ts, rtc_ts, diff;
  struct tm now;
  float ppm;

  get_rtc(fd, &rtc, &page4);

  rtc_ts = rtc_to_double(&page4,&now);

  printf("ss_d = %u ss = %u dt = %u y = %u\n", page4.subsecond_div, page4.subseconds, page4.datetime, page4.year);
  printf("%u:%u:%u %u/%u/%u\n", 
      now.tm_hour, now.tm_min, now.tm_sec,
      now.tm_mon+1, now.tm_mday, now.tm_year+1900
      );
  ppm = lse_calibration_to_ppm(page4.lse_calibration);
  printf("calib = %u (%.3f ppm) set = %u bk1 = %u bk2 = %u\n", page4.lse_calibration, ppm, page4.set_rtc, page4.backup_register[0], page4.backup_register[1]);
  printf("LSE: milli=%u tim2=%u tim14=%u\n", page4.LSE_millis_irq, page4.LSE_tim2_irq, page4.LSE_tim14_cap);

  local_ts = rtc.tv_sec + rtc.tv_usec / 1000000.0;
  diff = local_ts-rtc_ts;
  printf("%.6f %.3f %.3f %s\n", local_ts, rtc_ts, diff, (diff < 0) ? "fast" : "slow");
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

#define POLLS 3
static void sync_rtc(int fd) {
  struct timeval rtc[POLLS];
  struct i2c_registers_type_page4 page4[POLLS];
  double local_ts[POLLS], rtc_ts[POLLS], diff[POLLS], median;

  for(uint8_t i = 0; i < POLLS; i++) {
    get_rtc(fd, &rtc[i], &page4[i]);
    rtc_ts[i] = rtc_to_double(&page4[i],NULL);
    local_ts[i] = rtc[i].tv_sec + rtc[i].tv_usec / 1000000.0;
    diff[i] = local_ts[i]-rtc_ts[i]; // positive: rtc slow, negative: rtc fast
    printf("sample %d value %.3f\n", i, diff[i]);
    usleep(1500); 
  }

  median = median_d(diff, POLLS);

  printf("median %.4f\n", median);
  while(fabs(median) > 1) {
    if(median > 0) {
      // rtc 1s+ slow
      setsubsecond(fd, 1, 0);
      median -= 1;
      printf("set +1s\n");
    } else {
      // rtc 1s+ fast
      setsubsecond(fd, 0, 1024);
      median += 1;
      printf("set -1s\n");
    }
    usleep(1200000); // allow setsubsecond to finish
  }
  if(median > 0.0009) { 
    // rtc slow
    setsubsecond(fd, 1, 1024*(1-median));
    printf("set +1s -%.0f counts\n", 1024*(1-median));
  } else if(median < -0.0009) {
    // rtc fast
    setsubsecond(fd, 0, -1024*median);
    printf("set -%.0f counts\n", -1024*median);
  } else {
    printf("no change\n");
  }
}

static void setcalibration(int fd, uint32_t addclk, uint32_t subclk) {
  struct i2c_registers_type_page4 page4;
  uint8_t set_page[2];
  uint8_t set_page4_calib[3], set_page4_apply[2];

  page4.lse_calibration = (addclk ? CALIBRATION_ADDCLK : 0) | (subclk & CALIBRATION_SUBCLK_MASK);

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE4;

  set_page4_calib[0] = 8;
  memcpy(set_page4_calib+1, &page4.lse_calibration, sizeof(page4.lse_calibration));

  set_page4_apply[0] = 11;
  set_page4_apply[1] = SET_RTC_CALIBRATION;

  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  write_i2c(fd, set_page4_calib, sizeof(set_page4_calib));
  write_i2c(fd, set_page4_apply, sizeof(set_page4_apply));
  unlock_i2c(fd);

  printf("set to %u (%.3f ppm)\n", page4.lse_calibration, lse_calibration_to_ppm(page4.lse_calibration));
}

int main(int argc, char **argv) {
  int fd;

  setup_rtc_tz();

  fd = open_i2c(I2C_ADDR); 

  if(argc == 1) {
    printf("commands: get, set, setsubsecond, setcalibration, sync\n");
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

  printf("unknown command\n");
  exit(1);
}

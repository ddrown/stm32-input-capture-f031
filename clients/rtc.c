#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "i2c.h"
#include "i2c_registers.h"
#include "rtc_data.h"

int main(int argc, char **argv) {
  struct i2c_registers_type_page4 page4;
  int fd;
  struct timeval rtc;

  setup_rtc_tz();

  fd = open_i2c(I2C_ADDR); 

  if(argc == 1) {
    printf("commands: get, set\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    double local_ts, rtc_ts;
    struct tm now;
    get_rtc(fd, &rtc, &page4);

    rtc_ts = rtc_to_double(&page4,&now);

    printf("ss_d = %u ss = %u dt = %u y = %u\n", page4.subsecond_div, page4.subseconds, page4.datetime, page4.year);
    printf("%u:%u:%u %u/%u/%u\n", 
	now.tm_hour, now.tm_min, now.tm_sec,
	now.tm_mon+1, now.tm_mday, now.tm_year+1900
	);
    printf("calib = %u set = %u bk1 = %u bk2 = %u\n", page4.lse_calibration, page4.set_rtc, page4.backup_register[0], page4.backup_register[1]);
    printf("LSE: milli=%u tim2=%u tim14=%u\n", page4.LSE_millis_irq, page4.LSE_tim2_irq, page4.LSE_tim14_cap);

    local_ts = rtc.tv_sec + rtc.tv_usec / 1000000.0;
    printf("%.6f %.3f %.3f\n", local_ts, rtc_ts, local_ts-rtc_ts);
  }

  if(strcmp(argv[1], "set") == 0) { // TODO
  }
}

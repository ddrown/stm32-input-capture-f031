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

int main() {
  int fd;
  struct i2c_registers_type i2c_registers;
  struct i2c_registers_type_page2 i2c_registers_page2;
  struct i2c_registers_type_page4 i2c_registers_page4;
  struct timeval local;

  setup_rtc_tz();

  fd = open_i2c(I2C_ADDR); 

  printf("time rtc lse/2 lse/14 ch1 ch1# ch2 ch2# ch3 ch3# ch4 ch4# ");
  adc_header();
  printf("\n");
  while(1) {
    double local_ts, rtc_ts;

    get_i2c_structs(fd, &i2c_registers, &i2c_registers_page2);
    get_rtc(fd, &local, &i2c_registers_page4);
    add_adc_data(&i2c_registers, &i2c_registers_page2);

    local_ts = local.tv_sec + local.tv_usec / 1000000.0;
    rtc_ts = rtc_to_double(&i2c_registers_page4,NULL);

    printf("%lu %.3f %u %u ", local.tv_sec, local_ts - rtc_ts, i2c_registers_page4.LSE_tim2_irq, i2c_registers_page4.LSE_tim14_cap);
    for(uint8_t i = 0; i < 4; i++) {
      printf("%u %u ", i2c_registers.tim2_at_cap[i], i2c_registers.ch_count[i]);
    }
    adc_print();
    printf("\n");
    fflush(stdout);

    sleep(1);
  }
}

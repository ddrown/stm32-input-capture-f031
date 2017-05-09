#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>

#include "i2c_registers.h"
#include "rtc_data.h"

double rtc_to_double(struct i2c_registers_type_page4 *page4, struct tm *now) {
  time_t now_t;
  struct tm now2;
  double timestamp;

  if(now == NULL)
    now = &now2;

  now->tm_sec =  (page4->datetime & 0b00000000000000000111111000000000) >> 9;
  now->tm_min =  (page4->datetime & 0b00000000000111111000000000000000) >> 15;
  now->tm_hour = (page4->datetime & 0b00000011111000000000000000000000) >> 21;
  now->tm_mon = ((page4->datetime & 0b00000000000000000000000111100000) >> 5) - 1;
  now->tm_mday = (page4->datetime & 0b00000000000000000000000000011111);
  now->tm_year = 100 + page4->year;
  if(page4->year < 17) {
    now->tm_year += 100; // valid years: 2017..2116
  }
  now->tm_wday = 0;
  now->tm_yday = 0;
  now->tm_isdst = 0;

  now_t = timegm(now);

  timestamp = now_t + (page4->subsecond_div - page4->subseconds) / (double)page4->subsecond_div;
  return timestamp;
}

void tm_to_rtc(struct i2c_registers_type_page4 *page4, struct tm *now) {
  page4->year = now->tm_year - 100;
  if(page4->year > 99)
    page4->year = page4->year - 100; // good till 2200
  page4->datetime = 
    ((uint32_t)now->tm_mday) | ((uint32_t)now->tm_mon + 1) << 5 |
    ((uint32_t)now->tm_sec) << 9 | ((uint32_t)now->tm_min) << 15 |
    ((uint32_t)now->tm_hour) << 21;
}

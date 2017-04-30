#ifndef RTC_DATA_H
#define RTC_DATA_H

void setup_rtc_tz();
double rtc_to_double(struct i2c_registers_type_page4 *page4, struct tm *now);

#endif

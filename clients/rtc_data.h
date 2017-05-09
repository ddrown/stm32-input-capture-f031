#ifndef RTC_DATA_H
#define RTC_DATA_H

double rtc_to_double(struct i2c_registers_type_page4 *page4, struct tm *now);
void tm_to_rtc(struct i2c_registers_type_page4 *page4, struct tm *now);

#endif

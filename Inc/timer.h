#ifndef TIMER_H
#define TIMER_H

extern RTC_HandleTypeDef hrtc;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim14;

#define DEFAULT_SOURCE_HZ 50

void timer_start();
void set_rtc_registers();
void set_rtc(uint8_t data);

int32_t timer_ppt();
void adjust_pps();
void set_frequency(int32_t frequency);
void add_offset(int32_t offset);

void set_page5_registers();
void rtc_start();

#endif

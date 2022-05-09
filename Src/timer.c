#include "stm32f0xx_hal.h"

#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "timer.h"
#include "uart.h"
#include "i2c_slave.h"
#include "flash.h"

#define LSE_DIVIDED_FREQ 32
static uint8_t lse_counts = LSE_DIVIDED_FREQ;

// in hundreds of picoseconds, range +/-214ms
// 48MHz, in hundreds of picoseconds
#define PS_PER_COUNT 208
#define DEFAULT_RELOAD 47999999
// limit movement to about +/-312 ppm
#define MAX_ADJUST 15000

static uint32_t lastadjust = DEFAULT_RELOAD;
static enum {PRESCALE_NORMAL, PRESCALE_PENDING, PRESCALE_ACTIVE} pending_prescale = PRESCALE_NORMAL;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if(pending_prescale == PRESCALE_PENDING) {
    htim2.Instance->ARR = lastadjust;
    pending_prescale = PRESCALE_ACTIVE;
  } else if(pending_prescale == PRESCALE_ACTIVE) {
    htim2.Instance->ARR = DEFAULT_RELOAD;
    pending_prescale = PRESCALE_NORMAL;
  }
}

void adjust_pps() {
  int32_t offset_count;

  i2c_registers.last_ppt = timer_ppt();
  // in hundreds of picoseconds, adjust offset by expected freq
  i2c_registers.offset_ps += (i2c_registers.last_ppt + i2c_registers.static_ppt) / 100;
  offset_count = i2c_registers.offset_ps / PS_PER_COUNT;

  if(offset_count > MAX_ADJUST)
    offset_count = MAX_ADJUST;
  if(offset_count < -1*MAX_ADJUST)
    offset_count = -1*MAX_ADJUST;

  // remove the adjusted part, leave the rest
  i2c_registers.offset_ps -= offset_count * PS_PER_COUNT;
  lastadjust = DEFAULT_RELOAD - offset_count;
  i2c_registers.lastadjust = lastadjust;
  pending_prescale = PRESCALE_PENDING;
}

// TIM2&TIM14 input capture 
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  const uint32_t tim2_at_irq = __HAL_TIM_GET_COUNTER(&htim2);
  const uint32_t milliseconds_irq = HAL_GetTick();

  if(htim == &htim14) {
    if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
      lse_counts--;
      if(lse_counts == 0) {
        i2c_registers_page4.LSE_tim14_cap = HAL_TIM_ReadCapturedValue(&htim14, TIM_CHANNEL_1);
        i2c_registers_page4.LSE_tim2_irq = tim2_at_irq;
        i2c_registers_page4.LSE_millis_irq = milliseconds_irq;
        lse_counts = LSE_DIVIDED_FREQ;
      }
    }
  }
}

// caller should limit offset to prevent wraps
void add_offset(int32_t offset) {
  i2c_registers.offset_ps += offset;
}

void set_frequency(int32_t frequency) {
  i2c_registers.static_ppt = frequency;
}

void timer_start() {
  HAL_TIM_Base_Start_IT(&htim2);
#ifdef HAT_CH1
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
#elif HAT_CH2
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
#else
#error choose which output channel to use
#endif
  HAL_TIM_Base_Start(&htim14);
  HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
}

// these values are from a specific TCXO, you'll need to measure your own hardware for best results.
// fixed-point math is used to save flash space, Cortex-M0 uses software floating point
// most terms are multipled by 1e6
// g(x) = TEMP_ADJUST_A + TEMP_ADJUST_B * (x - TEMP_ADJUST_D) + ((x - TEMP_ADJUST_D)**2) / TEMP_ADJUST_C

// limits: +/- 2,147 ppm
int32_t timer_ppt() {
  if (tcxo_calibration.tcxo_d == 0)
    return 0;

  const int32_t temp_uF = i2c_registers_page2.internal_temp_mF * 1000;
  const int64_t ppt_a64 = tcxo_calibration.tcxo_b * (int64_t)(temp_uF - tcxo_calibration.tcxo_d) / 1000000;
  const int64_t ppt_b64 = ((int64_t)(temp_uF - tcxo_calibration.tcxo_d))*(temp_uF - tcxo_calibration.tcxo_d) / tcxo_calibration.tcxo_c;
  const int64_t ppt = tcxo_calibration.tcxo_a + ppt_a64 + ppt_b64;

  return ppt;
}

void RTC_IRQHandler(void) {
  HAL_RTC_AlarmIRQHandler(&hrtc);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
  UNUSED(hrtc);

  i2c_registers_page4.tim2_rtc_second = __HAL_TIM_GET_COUNTER(&htim2);
}

void rtc_start() {
  RTC_AlarmTypeDef sAlarm;
  
  HAL_NVIC_SetPriority(RTC_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(RTC_IRQn);

  // RTC interrupt every second
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x0;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_ALL;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK) {
    Error_Handler();
  }
}

void set_rtc_registers() {
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  i2c_registers_page4.subseconds = sTime.SubSeconds;
  i2c_registers_page4.year = sDate.Year;
  i2c_registers_page4.datetime = 
    ((uint32_t)sDate.Date) | ((uint32_t)sDate.Month) << 5 |
    ((uint32_t)sTime.Seconds) << 9 | ((uint32_t)sTime.Minutes) << 15 |
    ((uint32_t)sTime.Hours) << 21;
}

void set_page5_registers() {
  i2c_registers_page5.cur_tim2 = __HAL_TIM_GET_COUNTER(&htim2);
  i2c_registers_page5.cur_millis = HAL_GetTick();
}

static void set_rtc_datetime() {
  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

  sTime.Hours =   (i2c_registers_page4.datetime & 0b00000011111000000000000000000000) >> 21;
  sTime.Minutes = (i2c_registers_page4.datetime & 0b00000000000111111000000000000000) >> 15;
  sTime.Seconds = (i2c_registers_page4.datetime & 0b00000000000000000111111000000000) >> 9;
  sTime.TimeFormat = 0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sDate.Month = (i2c_registers_page4.datetime & 0b00000000000000000000000111100000) >> 5;
  sDate.Date =  (i2c_registers_page4.datetime & 0b00000000000000000000000000011111);
  sDate.Year =  i2c_registers_page4.year;
  sDate.WeekDay = 1;
  HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 
  HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN); 
}

void set_rtc(uint8_t data) {
  switch(data) {
    case SET_RTC_DATETIME:
      set_rtc_datetime();
      break;
    case SET_RTC_CALIBRATION:
      HAL_RTCEx_SetSmoothCalib(&hrtc, RTC_SMOOTHCALIB_PERIOD_32SEC, i2c_registers_page4.lse_calibration & RTC_SMOOTHCALIB_PLUSPULSES_SET, i2c_registers_page4.lse_calibration & 0b111111111);
      break;
    case SET_RTC_SUBSECOND:
      HAL_RTCEx_SetSynchroShift(&hrtc,
          (i2c_registers_page4.subseconds & 0b1000000000000000) ? RTC_SHIFTADD1S_SET : RTC_SHIFTADD1S_RESET,
           i2c_registers_page4.subseconds & 0b0111111111111111
           );
      break;
    default:
      break;
  }
}

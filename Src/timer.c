#include "stm32f0xx_hal.h"

#include <stdlib.h>
#include <math.h>

#include "timer.h"
#include "uart.h"
#include "i2c_slave.h"

#define LSE_DIVIDED_FREQ 32
static uint8_t lse_counts = LSE_DIVIDED_FREQ;
static uint8_t counts_primary = DEFAULT_SOURCE_HZ;

static const uint8_t channel_values[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
static const uint16_t overflow_flags[] = {TIM_FLAG_CC1OF, TIM_FLAG_CC2OF, TIM_FLAG_CC3OF, TIM_FLAG_CC4OF};

static inline void tim2_channel(uint32_t channel, uint32_t milli_irq) {
  if(channel == i2c_registers.primary_channel) {
    counts_primary--;
    if(counts_primary == 0) {
      i2c_registers.milliseconds_irq_primary = milli_irq;
      i2c_registers.tim2_at_cap[channel] = HAL_TIM_ReadCapturedValue(&htim2, channel_values[channel]);

      if(i2c_registers.primary_channel_HZ > 0) {
        counts_primary = i2c_registers.primary_channel_HZ;
      } else {
        counts_primary = i2c_registers.primary_channel_HZ = DEFAULT_SOURCE_HZ;
      }
    }
    if(__HAL_TIM_GET_FLAG(&htim2, overflow_flags[channel])) { // there was an overflow event
      // don't consider this as the normal counts_primary, as it was probably noise
      __HAL_TIM_CLEAR_FLAG(&htim2, overflow_flags[channel]);
    }
  } else {
    i2c_registers.tim2_at_cap[channel] = HAL_TIM_ReadCapturedValue(&htim2, channel_values[channel]);
    i2c_registers.ch_count[channel]++;
    if(__HAL_TIM_GET_FLAG(&htim2, overflow_flags[channel])) { // there was an overflow event
      i2c_registers.ch_count[channel]++;
      __HAL_TIM_CLEAR_FLAG(&htim2, overflow_flags[channel]);
    }
  }
}

// TIM2&TIM14 input capture 
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  uint32_t tim2_at_irq;
  uint32_t milliseconds_irq;

  // TODO: save the difference between tim2_at_irq and tim2_at_cap in i2c_registers_page2?
  tim2_at_irq = __HAL_TIM_GET_COUNTER(&htim2);
  milliseconds_irq = HAL_GetTick();

  if(htim == &htim2) {
    switch(htim->Channel) {
      case HAL_TIM_ACTIVE_CHANNEL_1:
        tim2_channel(0, milliseconds_irq);
        break;
      case HAL_TIM_ACTIVE_CHANNEL_2:
        tim2_channel(1, milliseconds_irq);
        break;
      case HAL_TIM_ACTIVE_CHANNEL_3:
        tim2_channel(2, milliseconds_irq);
        break;
      case HAL_TIM_ACTIVE_CHANNEL_4:
        tim2_channel(3, milliseconds_irq);
        break;
      default:
        break;
    }
  } else if(htim == &htim14) {
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

void timer_start() {
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);
  HAL_TIM_Base_Start(&htim14);
  HAL_TIM_IC_Start_IT(&htim14, TIM_CHANNEL_1);
}

void print_timer_status() {
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

#include "stm32f0xx_hal.h"

#include <stdlib.h>
#include <math.h>

#include "timer.h"
#include "uart.h"
#include "i2c_slave.h"

// TIM3 input capture 
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  static uint8_t counts_ch1 = DEFAULT_SOURCE_HZ;
  uint32_t tim2_at_irq;
  uint32_t milliseconds_irq;

  // TODO: save the difference between tim2_at_irq and tim2_at_cap in i2c_registers_page2?
  tim2_at_irq = __HAL_TIM_GET_COUNTER(&htim2);
  milliseconds_irq = HAL_GetTick();

  // figure out where the input capture came from
  if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
    counts_ch1--;
    if(counts_ch1 == 0) {
      i2c_registers.milliseconds_irq_ch1 = milliseconds_irq;
      i2c_registers.tim2_at_cap[0] = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_1);

      if(i2c_registers.source_HZ_ch1 > 0) {
	counts_ch1 = i2c_registers.source_HZ_ch1;
      } else {
	counts_ch1 = i2c_registers.source_HZ_ch1 = DEFAULT_SOURCE_HZ;
      }
    }
    if(__HAL_TIM_GET_FLAG(htim, TIM_FLAG_CC1OF)) { // there was an overflow event
      // don't consider this as the normal counts_ch1, as it shouldn't happen at low frequencies
      __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC1OF);
    }
  } else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    i2c_registers.tim2_at_cap[1] = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_2);
    i2c_registers.ch2_count++;
    if(__HAL_TIM_GET_FLAG(htim, TIM_FLAG_CC2OF)) { // there was an overflow event
      i2c_registers.ch2_count++;
      __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC2OF);
    }
  } else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
    i2c_registers.tim2_at_cap[2] = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_3);
    i2c_registers.ch3_count++;
    if(__HAL_TIM_GET_FLAG(htim, TIM_FLAG_CC3OF)) { // there was an overflow event
      i2c_registers.ch3_count++;
      __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC3OF);
    }
  } else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
    i2c_registers.tim2_at_cap[3] = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_4);
    i2c_registers.ch4_count++;
    if(__HAL_TIM_GET_FLAG(htim, TIM_FLAG_CC4OF)) { // there was an overflow event
      i2c_registers.ch4_count++;
      __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_CC4OF);
    }
  }
}

void timer_start() {
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);
}

void print_timer_status() {
}

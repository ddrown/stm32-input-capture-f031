#include "stm32f0xx_hal.h"
#include <stdint.h>
#include "adc.h"
#include "i2c_slave.h"
#include "uart.h"

// 3.3V per ADC count of 4096 = 0.000805664 (value in nV)
#define NV_PER_COUNT 805664

#define AVERAGE_SAMPLES 10

static uint16_t internal_vbat[AVERAGE_SAMPLES];
static uint16_t internal_temps[AVERAGE_SAMPLES];
static uint16_t internal_vrefs[AVERAGE_SAMPLES];
static int8_t adc_index = -1;

#define AVERAGE_SAMPLES_CALC 60

static int32_t temps[AVERAGE_SAMPLES_CALC];
static uint64_t vccs[AVERAGE_SAMPLES_CALC];
static int8_t calc_index = -1;

// addresses from the STM32F030 datasheet
// internal_temp value at 30C+/-5C @3.3V+/-10mV
static uint16_t *ts_cal1 = (uint16_t *)0x1ffff7b8;
// internal_temp value at 110C+/-5C @3.3V+/-10mV
static uint16_t *ts_cal2 = (uint16_t *)0x1ffff7c2;
// internal_vref value at 30C+/-5C @3.3V+/-10mV
static uint16_t *vrefint_cal = (uint16_t *)0x1ffff7ba;

static uint16_t avg_16(uint16_t *values, uint8_t index) {
  uint32_t sum = 0;
  for(uint8_t i = 0; i <= index; i++) {
    sum += values[i];
  }
  return sum / (index+1);
}

static uint64_t avg_64(uint64_t *values, uint8_t index) {
  uint64_t sum = 0;
  for(uint8_t i = 0; i <= index; i++) {
    sum += values[i];
  }
  return sum / (index + 1);
}

static int32_t avg_i(int32_t *values, uint8_t index) {
  int64_t sum = 0;
  for(uint8_t i = 0; i <= index; i++) {
    sum += values[i];
  }
  return sum / (index + 1);
}

void adc_poll() {
  if(adc_index < (AVERAGE_SAMPLES-1)) {
    adc_index++;
  } else {
    for(uint8_t i = 0; i < (AVERAGE_SAMPLES-1); i++) {
      internal_temps[i] = internal_temps[i+1];
      internal_vrefs[i] = internal_vrefs[i+1];
      internal_vbat[i] = internal_vbat[i+1];
    }
  }

  // turn battery measurement on
  ADC1_COMMON->CCR |= ADC_CCR_VBATEN;
  HAL_ADC_Start(&hadc);
  if(HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
    internal_temps[adc_index] = HAL_ADC_GetValue(&hadc);
  }
  if(HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
    internal_vrefs[adc_index] = HAL_ADC_GetValue(&hadc);
  }
  if(HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
    internal_vbat[adc_index] = HAL_ADC_GetValue(&hadc);
  }
  HAL_ADC_Stop(&hadc);
  // turn battery measurement off so it isn't a constant 33uA drain
  ADC1_COMMON->CCR &= ~ADC_CCR_VBATEN;
}

void update_adc() {
  const uint16_t temp = avg_16(internal_temps, adc_index);
  const uint16_t vref = avg_16(internal_vrefs, adc_index);

  if(calc_index < (AVERAGE_SAMPLES_CALC-1)) {
    calc_index++;
  } else {
    for(uint8_t i = 0; i < (AVERAGE_SAMPLES_CALC-1); i++) {
      temps[i] = temps[i+1];
      vccs[i] = vccs[i+1];
    }
  }

  const uint32_t vref_expected_nv = (*vrefint_cal) * NV_PER_COUNT;
  const uint32_t vref_actual_nv = vref*NV_PER_COUNT;
  const uint64_t vcc_nv = (uint64_t)((vref_expected_nv+500)/1000)*3300000000/((vref_actual_nv+500)/1000);

  vccs[calc_index] = vcc_nv;
  const uint64_t last_vcc_nv = avg_64(vccs, calc_index);

  const uint32_t nv_per_count = last_vcc_nv / 4096;

  const uint32_t temp_nv = temp * nv_per_count;
  const uint32_t nv_30C = (*ts_cal1) * NV_PER_COUNT;
  const uint32_t nv_110C = (*ts_cal2) * NV_PER_COUNT;

  int32_t uv_per_C = (((int32_t)nv_110C - (int32_t)nv_30C) / (110-30) + 500)/1000;
  int32_t temp_mC = ((int32_t)temp_nv - (int32_t)nv_30C) / uv_per_C + 30000;
  int32_t temp_mF = temp_mC * 9/5+32000;
  temps[calc_index] = temp_mF;

  i2c_registers_page2.internal_vbat_mv = avg_16(internal_vbat, adc_index) * nv_per_count / 1000000;
  i2c_registers_page2.internal_temp_mF = avg_i(temps, calc_index);
  i2c_registers_page2.internal_vref_mv = last_vcc_nv / 1000000;
  i2c_registers_page2.last_adc_ms = HAL_GetTick();
}

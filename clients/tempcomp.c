#include <math.h>
#include <stdint.h>
#include <sys/types.h>

#include "i2c_registers.h"
#include "adc_calc.h"
#include "tempcomp.h"

// in ppb units
double tempcomp(const struct tempcomp_data *data) {
  float temp_f = last_temp()*9.0/5.0+32.0;
  return (data->tcxo_a + data->tcxo_b * (temp_f - data->tcxo_d) + data->tcxo_c * pow(temp_f - data->tcxo_d, 2)) * 1000.0;
}

#ifndef VREF_CALC_H
#define VREF_CALC_H

void add_power_data(const struct i2c_registers_type_page2 *i2c_registers_page2);
float last_vref();
float last_vbat();

#endif

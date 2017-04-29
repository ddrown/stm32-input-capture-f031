#ifndef I2C_SLAVE
#define I2C_SLAVE

extern I2C_HandleTypeDef hi2c1;

void i2c_slave_start();
void i2c_show_data();
uint8_t i2c_read_active();

#define I2C_REGISTER_PAGE_SIZE 32

#define I2C_REGISTER_OFFSET_HZ_HI 26
#define I2C_REGISTER_OFFSET_HZ_LO 27
#define I2C_REGISTER_OFFSET_PAGE 31

#define I2C_REGISTER_PAGE1 0
#define I2C_REGISTER_PAGE2 1
#define I2C_REGISTER_PAGE3 2
#define I2C_REGISTER_PAGE4 3

#define I2C_REGISTER_VERSION 3

#define SAVE_STATUS_NONE 0
#define SAVE_STATUS_OK 1
#define SAVE_STATUS_ERASE_FAIL 2
#define SAVE_STATUS_WRITE_FAIL 3

extern struct i2c_registers_type {
  // start 0 len 4
  uint32_t milliseconds_now;
  // start 4 len 4
  uint32_t milliseconds_irq_primary;
  // start 8 len 16
  uint32_t tim2_at_cap[4];
  // start 24 len 4
  uint8_t ch_count[4];
  // start 28 len 1
  uint8_t primary_channel;
  // start 29 len 1
  uint8_t primary_channel_HZ;
  // start 30 len 1
  uint8_t version;
  // start 31 len 1
  uint8_t page_offset;
} i2c_registers;

extern struct i2c_registers_type_page2 {
  uint32_t last_adc_ms;
  uint16_t internal_temp;
  uint16_t internal_vref;
  uint16_t internal_vbat;
  uint16_t ts_cal1;     // internal_temp value at 30C+/-5C @3.3V+/-10mV
  uint16_t ts_cal2;     // internal_temp value at 110C+/-5C @3.3V+/-10mV
  uint16_t vrefint_cal; // internal_vref value at 30C+/-5C @3.3V+/-10mV
  uint8_t reserved[15];
  uint8_t page_offset;
} i2c_registers_page2;

extern struct i2c_registers_type_page3 {
  /* tcxo_X variables are floats stored as:
   * byte 1: negative sign (1 bit), exponent bits 7-1
   * byte 2: exponent bit 0, mantissa bits 23-17
   * byte 3: mantissa bits 16-8
   * byte 4: mantissa bits 7-0
   * they describe the expected frequency error in ppm:
   * ppm = tcxo_a + tcxo_b * (F - tcxo_c) + tcxo_d * pow(F - tcxo_c, 2)
   * where F is the temperature from the internal_temp sensor in Fahrenheit
   */
  uint32_t tcxo_a;
  uint32_t tcxo_b;
  uint32_t tcxo_c;
  uint32_t tcxo_d;
  uint8_t max_calibration_temp; // F
  int8_t min_calibration_temp;  // F
  uint8_t rmse_fit;             // ppb
  uint8_t save;                 // 1=save new values to flash
  uint8_t save_status;          // see SAVE_STATUS_X

  uint8_t reserved[10];

  uint8_t page_offset;
} i2c_registers_page3;

#define SET_RTC_DATETIME 1
#define SET_RTC_CALIBRATION 2
#define SET_RTC_SUBSECOND 3

// rtc timestamp taken at page switch i2c read
extern struct i2c_registers_type_page4 {
  uint16_t subsecond_div; // number of counts in 1s
  uint16_t subseconds; // read: subseconds (downcount), write: bit 16: +1s, bit 15..0: -N counts/PREDIV_S

  uint16_t date; // bits 0..4: day, 5..8: month, 8..15: year
  uint16_t time; // bits 0..5: second, 6..11: minute, 12..16: hour

  uint16_t lse_calibration; // bits 0..8: -N counts/32s, 9: +1count/2^11 (+488.281ppm)
  uint16_t set_rtc; // write values SET_RTC_DATETIME, SET_RTC_CALIBRATION, SET_RTC_SUBSECOND
// ^^^ 12 bytes

  uint32_t backup_register[2]; // 2 of the 5 rtc backup registers

  uint32_t LSE_millis_irq; // 1pps LSE signal vs TCXO
  uint32_t LSE_tim2_irq;
  uint16_t LSE_tim14_cap;

  uint8_t reserved;
  uint8_t page_offset;
// ^^^ 12+20=32 bytes
} i2c_registers_page4;

#endif

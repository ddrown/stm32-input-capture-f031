#ifndef I2C_SLAVE
#define I2C_SLAVE

extern I2C_HandleTypeDef hi2c1;

void i2c_slave_start();
void i2c_show_data();
uint8_t i2c_read_active();

#define I2C_REGISTER_PAGE_SIZE 32

#define I2C_REGISTER_OFFSET_PAGE 31

#define I2C_REGISTER_PAGE1 0
#define I2C_REGISTER_PAGE2 1
#define I2C_REGISTER_PAGE3 2
#define I2C_REGISTER_PAGE4 3
#define I2C_REGISTER_PAGE5 4

#define I2C_REGISTER_VERSION 4

#define SAVE_STATUS_NONE 0
#define SAVE_STATUS_OK 1
#define SAVE_STATUS_ERASE_FAIL 2
#define SAVE_STATUS_WRITE_FAIL 3

extern struct i2c_registers_type {
  uint32_t milliseconds_now;
  int32_t offset_ps;
  int32_t static_ppt;
  int32_t last_ppt;
  uint32_t lastadjust;
  uint32_t reserved1[2];
  uint16_t reserved2;
  uint8_t version;
  uint8_t page_offset;
} i2c_registers;

extern struct i2c_registers_type_page2 {
  uint32_t last_adc_ms;
  int32_t internal_temp_mF;
  uint16_t internal_vref_mv;
  uint16_t internal_vbat_mv;
  uint8_t reserved[19];
  uint8_t page_offset;
} i2c_registers_page2;

extern struct i2c_registers_type_page3 {
  int32_t tcxo_a; // a * 10e6
  int32_t tcxo_b; // b * 10e6
  int64_t tcxo_c; // 1/c * -10e6
  int32_t tcxo_d; // d * 10e6
  uint8_t max_calibration_temp; // F
  int8_t min_calibration_temp;  // F
  uint8_t rmse_fit;             // for tcxo, in ppb
  uint8_t save;                 // 1=save new values to flash
  uint8_t save_status;          // see SAVE_STATUS_X

  uint8_t reserved[6]; // future: lse calibration, tcxo aging?

  uint8_t page_offset;
} i2c_registers_page3;

#define SET_RTC_DATETIME 1
#define SET_RTC_CALIBRATION 2
#define SET_RTC_SUBSECOND 3

// rtc timestamp taken at page switch i2c read
extern struct i2c_registers_type_page4 {
  uint16_t subsecond_div; // number of counts in 1s
  uint16_t subseconds; // read: subseconds (downcount), write: bit 15: +1s, bit 14..0: -N counts/PREDIV_S

  uint32_t datetime; // bits 0..4: day, 5..8: month, 9..14: second, 15..20: minute, 21..25: hour

  uint16_t lse_calibration; // bits 0..8: -N counts/32s, 9: +1count/2^11 (+488.281ppm)
  uint8_t year; // 0-99
  uint8_t set_rtc; // write values SET_RTC_DATETIME, SET_RTC_CALIBRATION, SET_RTC_SUBSECOND
// ^^^ 12 bytes

  uint32_t tim2_rtc_second;
  uint32_t reserved_0;

  uint32_t LSE_millis_irq; // 1pps LSE signal vs TCXO
  uint32_t LSE_tim2_irq;
  uint16_t LSE_tim14_cap;

  uint8_t reserved;
  uint8_t page_offset;
// ^^^ 12+20=32 bytes
} i2c_registers_page4;

// rtc timestamp taken at page switch i2c read
extern struct i2c_registers_type_page5 {
  uint32_t cur_tim2;
  uint32_t cur_millis;
  uint32_t backup_register[5];
  uint16_t reserved_0; // future: temp state at power on?
  uint8_t reserved_1;
  uint8_t page_offset;
} i2c_registers_page5;

#endif

#ifndef I2C_REGISTERS_H
#define I2C_REGISTERS_H

#define I2C_ADDR 0x4
#define EXPECTED_FREQ 48000000
#define INPUT_CHANNELS 4

// i2c interface
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

#define I2C_REGISTER_PRIMARY_CHANNEL 28
#define I2C_REGISTER_PRIMARY_CHANNEL_HZ 29

struct i2c_registers_type {
  uint32_t milliseconds_now;
  int32_t offset_ps;
  int32_t static_ppt;
  int32_t last_ppt;
  uint32_t lastadjust;
  uint32_t reserved1[2];
  uint16_t reserved2;
  uint8_t version;
  uint8_t page_offset;
};

struct i2c_registers_type_page2 {
  uint32_t last_adc_ms;
  int32_t internal_temp_mF;
  uint16_t internal_vref_mv;
  uint16_t internal_vbat_mv;
  uint8_t reserved[19];
  uint8_t page_offset;
};

// write from tcxo_a to save
#define I2C_PAGE3_WRITE_LENGTH 24
struct i2c_registers_type_page3 {
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
};

#define SET_RTC_DATETIME 1
#define SET_RTC_CALIBRATION 2
#define SET_RTC_SUBSECOND 3

#define CALIBRATION_ADDCLK      0b1000000000000000
#define CALIBRATION_SUBCLK_MASK 0b0000000111111111
#define SUBSECOND_ADD1S       0b1000000000000000
#define SUBSECOND_SUBCLK_MASK 0b0111111111111111

struct i2c_registers_type_page4 {
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
};

struct i2c_registers_type_page5 {
  uint32_t cur_tim2;
  uint32_t cur_millis;
  uint32_t backup_register[5];
  uint16_t reserved_0;
  uint8_t reserved_1;
  uint8_t page_offset;
};

void get_i2c_structs(int fd, struct i2c_registers_type *i2c_registers, struct i2c_registers_type_page2 *i2c_registers_page2);
void get_rtc(int fd, struct timeval *setpage, struct i2c_registers_type_page4 *i2c_registers_page4);
void get_timers(int fd, struct timespec *before_setpage, struct timespec *setpage, struct i2c_registers_type_page5 *i2c_registers_page5);
void get_i2c_page3(int fd, struct i2c_registers_type_page3 *i2c_registers_page3);
float last_i2c_time();

#endif

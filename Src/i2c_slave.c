#include "stm32f0xx_hal.h"
#include <string.h>

#include "i2c_slave.h"
#include "uart.h"
#include "timer.h"
#include "flash.h"

struct i2c_registers_type i2c_registers;
struct i2c_registers_type_page2 i2c_registers_page2;
struct i2c_registers_type_page3 i2c_registers_page3;
struct i2c_registers_type_page4 i2c_registers_page4;
struct i2c_registers_type_page5 i2c_registers_page5;

static void *current_page = &i2c_registers;
static uint8_t current_page_data[I2C_REGISTER_PAGE_SIZE];

static void i2c_data_xmt(I2C_HandleTypeDef *hi2c);

static uint8_t i2c_transfer_position;
static enum {STATE_WAITING, STATE_GET_ADDR, STATE_GET_DATA, STATE_SEND_DATA, STATE_DROP_DATA} i2c_transfer_state;
static uint8_t i2c_data;

void i2c_slave_start() {
  memset(&i2c_registers, '\0', sizeof(i2c_registers));

  memset(&i2c_registers_page2, '\0', sizeof(i2c_registers_page2));

  memset(&i2c_registers_page3, '\0', sizeof(i2c_registers_page3));

  memset(&i2c_registers_page4, '\0', sizeof(i2c_registers_page4));

  memset(&i2c_registers_page5, '\0', sizeof(i2c_registers_page5));

  i2c_registers.page_offset = I2C_REGISTER_PAGE1;
  i2c_registers.version = I2C_REGISTER_VERSION;
  memcpy(current_page_data, current_page, I2C_REGISTER_PAGE_SIZE);

  i2c_registers_page2.page_offset = I2C_REGISTER_PAGE2;

  memcpy(&i2c_registers_page3, &tcxo_calibration, sizeof(tcxo_calibration));
  i2c_registers_page3.page_offset = I2C_REGISTER_PAGE3;
  i2c_registers_page3.save_status = SAVE_STATUS_NONE;

  i2c_registers_page4.page_offset = I2C_REGISTER_PAGE4;
  i2c_registers_page4.subsecond_div = (hrtc.Instance->PRER & RTC_PRER_PREDIV_S);
  i2c_registers_page4.lse_calibration = 0xffff & hrtc.Instance->CALR;

  i2c_registers_page5.page_offset = I2C_REGISTER_PAGE5;
  for(uint8_t i = 0; i < 5; i++) {
    i2c_registers_page5.backup_register[i] = HAL_RTCEx_BKUPRead(&hrtc, i);
  }

  HAL_I2C_EnableListen_IT(&hi2c1);
}

uint8_t i2c_read_active() {
  return (i2c_transfer_state == STATE_SEND_DATA) || (i2c_transfer_state == STATE_GET_ADDR);
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode) {
  if(TransferDirection == I2C_DIRECTION_TRANSMIT) { // master transmit
    i2c_transfer_state = STATE_GET_ADDR;
    HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &i2c_data, 1, I2C_FIRST_FRAME);
  } else {
    i2c_transfer_state = STATE_SEND_DATA;
    i2c_data_xmt(hi2c);
  }
}

static void change_page(uint8_t data) {
  switch(data) {
    case I2C_REGISTER_PAGE1:
      current_page = &i2c_registers;
      i2c_registers.milliseconds_now = HAL_GetTick();
      break;
    case I2C_REGISTER_PAGE2:
      current_page = &i2c_registers_page2;
      break;
    case I2C_REGISTER_PAGE3:
      current_page = &i2c_registers_page3;
      break;
    case I2C_REGISTER_PAGE4:
      current_page = &i2c_registers_page4;
      set_rtc_registers();
      break;
    case I2C_REGISTER_PAGE5:
      current_page = &i2c_registers_page5;
      set_page5_registers();
      break;
    default:
      current_page = &i2c_registers;
      break;
  }

  __disable_irq(); // copy with interrupts off to prevent the page's data from changing during read
  memcpy(current_page_data, current_page, I2C_REGISTER_PAGE_SIZE);
  __enable_irq();
}

static void i2c_data_rcv(uint8_t position, uint8_t data) {
  if(position == I2C_REGISTER_OFFSET_PAGE) {
    change_page(data);
    return;
  }
  if(position >= I2C_REGISTER_PAGE_SIZE) {
    return;
  }

  if(current_page == &i2c_registers) {
    static union {
      uint8_t bytes[4];
      int32_t setting;
    } temporary_set;

    // set offset_ps
    if (position > 3 && position < 7) {
      temporary_set.bytes[position % 4] = data;
    } else if (position == 7) {
      temporary_set.bytes[3] = data;
      add_offset(temporary_set.setting);

    // set static_ppt
    } else if (position > 7 && position < 11) {
      temporary_set.bytes[position % 4] = data;
    } else if (position == 11) {
      temporary_set.bytes[3] = data;
      set_frequency(temporary_set.setting);
    }

    return;
  } 
  
  if(current_page == &i2c_registers_page3) {
    uint8_t *p = (uint8_t *)&i2c_registers_page3;

    if(position < 23) {
      p[position] = data;
    } else if(position == 23 && data) {
      write_flash_data();
    }

    return;
  }

  if(current_page == &i2c_registers_page4) {
    uint8_t *p = (uint8_t *)&i2c_registers_page4;

    if(position > 1 && position < 11) {
      p[position] = data;
    } else if(position == 11) {
      set_rtc(data);
    }

    return;
  }

  if(current_page == &i2c_registers_page5) {
    uint8_t *p = (uint8_t *)&i2c_registers_page5;

    if(position > 7 && position < 28) {
      p[position] = data;
      switch(position) { // write to register on last byte write
        case 11:
          HAL_RTCEx_BKUPWrite(&hrtc, 0, i2c_registers_page5.backup_register[0]);
          break;
        case 15:
          HAL_RTCEx_BKUPWrite(&hrtc, 1, i2c_registers_page5.backup_register[1]);
          break;
        case 19:
          HAL_RTCEx_BKUPWrite(&hrtc, 2, i2c_registers_page5.backup_register[2]);
          break;
        case 23:
          HAL_RTCEx_BKUPWrite(&hrtc, 3, i2c_registers_page5.backup_register[3]);
          break;
        case 27:
          HAL_RTCEx_BKUPWrite(&hrtc, 4, i2c_registers_page5.backup_register[4]);
          break;
      }
    }

    return;
  }
}

static void i2c_data_xmt(I2C_HandleTypeDef *hi2c) {
  HAL_I2C_Slave_Sequential_Transmit_IT(hi2c, current_page_data, I2C_REGISTER_PAGE_SIZE, I2C_FIRST_FRAME);
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if(i2c_transfer_state == STATE_DROP_DATA) { // encountered a stop/error condition
    return;
  }

  i2c_data = 0;
  HAL_I2C_Slave_Sequential_Transmit_IT(hi2c, &i2c_data, 1, I2C_NEXT_FRAME);
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  switch(i2c_transfer_state) {
    case STATE_GET_ADDR:
      i2c_transfer_position = i2c_data;
      i2c_transfer_state = STATE_GET_DATA;
      HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &i2c_data, 1, I2C_LAST_FRAME);
      break;
    case STATE_GET_DATA:
      i2c_data_rcv(i2c_transfer_position, i2c_data);
      i2c_transfer_position++;
      HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &i2c_data, 1, I2C_LAST_FRAME);
      break;
    default:
      HAL_I2C_Slave_Sequential_Receive_IT(hi2c, &i2c_data, 1, I2C_LAST_FRAME);
      break;
  }

  if(i2c_transfer_position > sizeof(struct i2c_registers_type)) {
    i2c_transfer_state = STATE_DROP_DATA;
  }
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c) {
  HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  i2c_transfer_state = STATE_DROP_DATA;

  if(hi2c->ErrorCode != HAL_I2C_ERROR_AF) {
    HAL_I2C_EnableListen_IT(hi2c);
  }
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c) {
  i2c_transfer_state = STATE_DROP_DATA;
}

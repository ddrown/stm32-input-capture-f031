#include "stm32f0xx_hal.h"

#include "flash.h"
#include "i2c_slave.h"

// stored in rwflash section, the whole 1024 byte page has to be erased/rewritten at the same time
uint32_t tcxo_calibration[8] __attribute__((section(".rwflash")));

void write_flash_data() {
  i2c_registers_page3.save_status = SAVE_STATUS_NONE;

  uint32_t addr = (uint32_t)tcxo_calibration; // assumption: tcxo_calibration starts at top of page
  FLASH_EraseInitTypeDef eraseRWFlash = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .PageAddress = addr,
    .NbPages = 1
  };
  uint32_t PageError;
  HAL_StatusTypeDef status;
  // flash is written 16 bits at a time
  uint16_t *d = (uint16_t *)&i2c_registers_page3;

  HAL_FLASH_Unlock();
  status = HAL_FLASHEx_Erase(&eraseRWFlash, &PageError);
  if(status != HAL_OK) {
    i2c_registers_page3.save_status = SAVE_STATUS_ERASE_FAIL;
    HAL_FLASH_Lock();
    return;
  }

  for(uint8_t i = 0; i < sizeof(i2c_registers_page3)/2; i++) {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i*2, d[i]);
    if(status != HAL_OK) {
      i2c_registers_page3.save_status = SAVE_STATUS_WRITE_FAIL;
      HAL_FLASH_Lock();
      return;
    }
  }

  HAL_FLASH_Lock();

  i2c_registers_page3.save_status = SAVE_STATUS_OK;
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "i2c_registers.h"

void page1(uint8_t *pagedata) {
  struct i2c_registers_type page;
  memcpy(&page, pagedata, sizeof(page));

  printf("ms = %u, offset = %d, conf_ppt = %d, last_ppt = %d\n", page.milliseconds_now, page.offset_ps, page.static_ppt, page.last_ppt);
  printf("adjust = %u, r=[%u,%u,%u], version=%u, offset = %u\n", page.lastadjust, page.reserved1[0], page.reserved1[1], page.reserved2, page.version, page.page_offset);
}

void page2(uint8_t *pagedata) {
  struct i2c_registers_type_page2 page;
  memcpy(&page, pagedata, sizeof(page));

  printf("last adc = %u, temp_mF = %u, vref_mv = %u, vbat_mv = %u\n", page.last_adc_ms, page.internal_temp_mF, page.internal_vref_mv, page.internal_vbat_mv);
  printf("r = [");
  for(int i = 0; i < 19; i++) {
    printf("%u ", page.reserved[i]);
  }
  printf("]\n");
  printf("offset = %u\n", page.page_offset);
}

void page3(uint8_t *pagedata) {
  struct i2c_registers_type_page3 page;
  memcpy(&page, pagedata, sizeof(page));

  printf("a = %d b = %d c = %ld d = %d maxT=%u minT=%d\n", page.tcxo_a, page.tcxo_b, page.tcxo_c, page.tcxo_d, page.max_calibration_temp, page.min_calibration_temp);
  printf("fit = %u save = %u status = %u offset = %u\n", page.rmse_fit, page.save, page.save_status, page.page_offset);
  printf("r = [");
  for(int i = 0; i < 6; i++) {
    printf("%u ", page.reserved[i]);
  }
  printf("]\n");
}

void page4(uint8_t *pagedata) {
  struct i2c_registers_type_page4 page;
  memcpy(&page, pagedata, sizeof(page));

  printf("subdiv = %u sub = %u date = %u lse_calib = %u year = %u set_rtc = %u\n", page.subsecond_div, page.subseconds, page.datetime, page.lse_calibration, page.year, page.set_rtc);
  printf("tim2_sec = %u r1 = %u RTC_ms = %u RTC_tim2 = %u RTC_tim14 = %u\n", page.tim2_rtc_second, page.reserved_0, page.LSE_millis_irq, page.LSE_tim2_irq, page.LSE_tim14_cap);
  printf("r2 = %u offset = %u\n", page.reserved, page.page_offset);
}

void page5(uint8_t *pagedata) {
  struct i2c_registers_type_page5 page;
  memcpy(&page, pagedata, sizeof(page));

  printf("cur2 = %u curms = %u backup = [%u,%u,%u,%u,%u] r0=%u r1=%u offset=%u\n", 
      page.cur_tim2, page.cur_millis, page.backup_register[0], page.backup_register[1], page.backup_register[2], page.backup_register[3], page.backup_register[4], page.reserved_0,
      page.reserved_1, page.page_offset);
}

int main(int argc, char **argv) {
  int fd;

  fd = open_i2c(I2C_ADDR); 

  for(int i = 0; i < 5; i++) {
    uint8_t set_page[2];
    uint8_t pagedata[32];

    printf("----[page %d\n", i+1);
    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = i;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, pagedata, sizeof(pagedata));
    unlock_i2c(fd);
    for(int j = 0; j < sizeof(pagedata); j++) {
      printf("%02x", pagedata[j]);
    }
    printf("\n");
    if (i == 0) {
      page1(pagedata);
    } else if (i == 1) {
      page2(pagedata);
    } else if (i == 2) {
      page3(pagedata);
    } else if (i == 3) {
      page4(pagedata);
    } else if (i == 4) {
      page5(pagedata);
    }
  }
}


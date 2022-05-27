#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "i2c.h"
#include "i2c_registers.h"

int main(int argc, char **argv) {
  int fd;
  struct i2c_registers_type page1;
  struct i2c_registers_type_page2 page2;
  struct i2c_registers_type_page5 page5;
  uint8_t set_page[2];

  fd = open_i2c(I2C_ADDR); 

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  while(1) {
    uint32_t next;

    set_page[1] = 0;

    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, &page1, sizeof(page1));

    set_page[1] = 1;
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, &page2, sizeof(page2));

    set_page[1] = 4;
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, &page5, sizeof(page5));

    unlock_i2c(fd);

    printf("%ld %d %d %u %u %u %u\n", time(NULL), page1.offset_ps, page1.last_ppt, page2.internal_temp_mF, page2.internal_vref_mv, page2.internal_vbat_mv, page5.cur_tim2);

    next = (page1.lastadjust - page5.cur_tim2) / 48 + 500000;
    fflush(stdout);
    usleep(next);
  }
}


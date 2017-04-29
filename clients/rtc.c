#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "i2c_registers.h"

int main(int argc, char **argv) {
  struct i2c_registers_type_page4 page4;
  int fd;

  fd = open_i2c(I2C_ADDR); 

  if(argc == 1) {
    printf("commands: get, set\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    uint8_t set_page[2];

    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = I2C_REGISTER_PAGE4;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, &page4, sizeof(page4));
    unlock_i2c(fd);

    printf("ss_d = %u ss = %u dt = %u y = %u\n", page4.subsecond_div, page4.subseconds, page4.datetime, page4.year);
    printf("%u:%u:%u %u/%u/%u\n", 
	(page4.datetime & 0b00000011111000000000000000000000) >> 21,
	(page4.datetime & 0b00000000000111111000000000000000) >> 15,
	(page4.datetime & 0b00000000000000000111111000000000) >> 9,
	(page4.datetime & 0b00000000000000000000000000011111),
	(page4.datetime & 0b00000000000000000000000111100000) >> 5,
	page4.year
	);
    printf("calib = %u set = %u bk1 = %u bk2 = %u\n", page4.lse_calibration, page4.set_rtc, page4.backup_register[0], page4.backup_register[1]);
    printf("LSE: milli=%u tim2=%u tim14=%u\n", page4.LSE_millis_irq, page4.LSE_tim2_irq, page4.LSE_tim14_cap);
  }

  if(strcmp(argv[1], "set") == 0) { // TODO
  }
}

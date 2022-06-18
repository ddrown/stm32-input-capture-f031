#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "i2c.h"
#include "i2c_registers.h"

void set_offset(int fd, int32_t new_offset) {
  uint8_t set_page[2];
  uint8_t set_page3[5];

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE1;
  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  set_page3[0] = 4; // write offset
  memcpy(set_page3+1, &new_offset, sizeof(new_offset));
  write_i2c(fd, set_page3, sizeof(new_offset)+1);
  unlock_i2c(fd);
}

void set_freq(int fd, int32_t new_freq) {
  uint8_t set_page[2];
  uint8_t set_page3[5];

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE1;
  lock_i2c(fd);
  write_i2c(fd, set_page, sizeof(set_page));
  set_page3[0] = 8; // write offset
  memcpy(set_page3+1, &new_freq, sizeof(new_freq));
  write_i2c(fd, set_page3, sizeof(new_freq)+1);
  unlock_i2c(fd);
}

int32_t aging() {
  // rms of residuals      (FIT_STDFIT) = sqrt(WSSR/ndf)    : 8.72006 ppb
  return -1 + -0.000060712 * (time(NULL)-1593150000);
}

int main(int argc, char **argv) {
  int fd;
  struct i2c_registers_type page1;
  struct i2c_registers_type_page2 page2;

  fd = open_i2c(I2C_ADDR); 

  if(argc < 2) {
    printf("commands: get, set-offset, set-freq, aging\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    get_i2c_structs(fd, &page1, &page2);

    printf("offset = %d freq = %d\n", page1.offset_ps, page1.static_ppt);
    return 0;
  }

  if(strcmp(argv[1], "set-offset") == 0) {
    if(argc != 3) {
      printf("set arguments: offset(ns)\n");
      exit(1);
    }

    set_offset(fd, atoi(argv[2]));

    return 0;
  }

  if(strcmp(argv[1], "set-freq") == 0) {
    if(argc != 3) {
      printf("set arguments: freq(ppt)\n");
      exit(1);
    }

    set_freq(fd, atoi(argv[2]));

    return 0;
  }

  if(strcmp(argv[1], "aging") == 0) {
    while(1) {
      int32_t new_age = aging();

      printf("%lu %d\n", time(NULL), new_age);
      fflush(stdout);
      set_freq(fd, new_age);
      sleep(60);
    }
  }

  printf("unknown command\n");
  return 1;
}

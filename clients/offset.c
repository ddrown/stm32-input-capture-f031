#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "i2c_registers.h"

int main(int argc, char **argv) {
  int fd;
  struct i2c_registers_type page1;
  struct i2c_registers_type_page2 page2;

  fd = open_i2c(I2C_ADDR); 

  if(argc < 2) {
    printf("commands: get, set\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    get_i2c_structs(fd, &page1, &page2);

    printf("offset = %d freq = %d\n", page1.offset_ps, page1.static_ppt);
    return 0;
  }

  if(strcmp(argv[1], "set") == 0) {
    uint8_t set_page[2];
    uint8_t set_page3[5];
    int32_t new_offset;

    if(argc != 3) {
      printf("set arguments: offset(ns)\n");
      exit(1);
    }

    new_offset = atoi(argv[2]);

    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = I2C_REGISTER_PAGE1;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    set_page3[0] = 4; // write offset
    memcpy(set_page3+1, &new_offset, sizeof(new_offset));
    write_i2c(fd, set_page3, sizeof(new_offset)+1);
    unlock_i2c(fd);

    return 0;
  }
}

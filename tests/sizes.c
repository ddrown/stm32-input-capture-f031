#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
  int testing;
} I2C_HandleTypeDef;

#include "i2c_slave.h"

#define SIZE(COUNT, STRUCT) test_size(COUNT, sizeof(struct STRUCT), offsetof(struct STRUCT, page_offset))

int test_size(int page_count, ssize_t struct_size, ssize_t page_offset) {
  int found = 0;
  if (struct_size != 32) {
    printf("%d size %lu != 32\n", page_count, struct_size);
    found++;
  }
  if (page_offset != 31) {
    printf("%d offset %lu != 31\n", page_count, page_offset);
    found++;
  }
  return found;
}

int main() {
  int found = 0;
  found += SIZE(1, i2c_registers_type);
  found += SIZE(2, i2c_registers_type_page2);
  found += SIZE(3, i2c_registers_type_page3);
  found += SIZE(4, i2c_registers_type_page4);
  found += SIZE(5, i2c_registers_type_page5);
  exit(found != 0);
}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "i2c.h"
#include "i2c_registers.h"

static char *save_status_names[] = {"none", "ok", "erase fail", "write fail"};

const char *save_status_str(uint8_t save_status) {
  if(save_status <= SAVE_STATUS_WRITE_FAIL) {
    return save_status_names[save_status];
  }

  return "??";
}

int main(int argc, char **argv) {
  struct i2c_registers_type_page3 page3;
  int fd;

  fd = open_i2c(I2C_ADDR); 

  if(argc < 2) {
    printf("commands: get, set\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    get_i2c_page3(fd, &page3);
    float a, b, c, d;

    a = page3.tcxo_a / 1000000.0;
    b = page3.tcxo_b / 1000000.0;
    c = 1000000.0 / page3.tcxo_c;
    d = page3.tcxo_d / 1000000.0;

    printf("a = %g, b = %g, c = %g, d = %g\n", a, b, c, d);
    printf("raw a = %d, b = %d, c = %" PRId64 ", d = %d\n", page3.tcxo_a, page3.tcxo_b, page3.tcxo_c, page3.tcxo_d);
    printf("max = %u F min = %d F\n", page3.max_calibration_temp, page3.min_calibration_temp);
    printf("rmse = %u ppb\n", page3.rmse_fit);
    printf("save: %u status: %s (%u)\n", page3.save, save_status_str(page3.save_status), page3.save_status);
  }

  if(strcmp(argv[1], "set") == 0) {
    uint8_t set_page[2];
    uint8_t set_page3[33];

    if(argc != 9) {
      printf("set arguments: a b c d max min rmse\n");
      exit(1);
    }

    page3.tcxo_a = strtof(argv[2],NULL) * 1000000;
    page3.tcxo_b = strtof(argv[3],NULL) * 1000000;
    page3.tcxo_c = 1000000.0 / strtof(argv[4],NULL);
    page3.tcxo_d = strtof(argv[5],NULL) * 1000000;

    page3.max_calibration_temp = atoi(argv[6]);
    page3.min_calibration_temp = atoi(argv[7]);
    page3.rmse_fit = atoi(argv[8]);
    page3.save = 1;

    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = I2C_REGISTER_PAGE3;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    set_page3[0] = 0; // offset 0
    memcpy(set_page3+1, &page3, I2C_PAGE3_WRITE_LENGTH);
    write_i2c(fd, set_page3, I2C_PAGE3_WRITE_LENGTH+1);
    unlock_i2c(fd);
  }
}

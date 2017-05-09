#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2c.h"
#include "float.h"
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

  if(argc == 1) {
    printf("commands: get, set, get-primary, set-primary\n");
    exit(1);
  }

  if(strcmp(argv[1], "get") == 0) {
    struct tempcomp_data data;

    get_i2c_page3(fd, &page3, &data);

    printf("a = %g, b = %g, c = %g, d = %g\n", data.tcxo_a, data.tcxo_b, data.tcxo_c, data.tcxo_d);
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

    page3.tcxo_a = htonf(strtof(argv[2],NULL));
    page3.tcxo_b = htonf(strtof(argv[3],NULL));
    page3.tcxo_c = htonf(strtof(argv[4],NULL));
    page3.tcxo_d = htonf(strtof(argv[5],NULL));

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

  if(strcmp(argv[1], "set-primary") == 0) {
    uint8_t set_page[2];
    uint8_t set_page1[3];

    if(argc != 4) {
      printf("set-primary arguments: ch hz\n");
      exit(1);
    }

    set_page1[0] = I2C_REGISTER_PRIMARY_CHANNEL;
    set_page1[1] = atoi(argv[2])-1; // channel 1 = 0
    set_page1[2] = atoi(argv[3]); // hz

    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = I2C_REGISTER_PAGE1;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    write_i2c(fd, set_page1, sizeof(set_page1));
    unlock_i2c(fd);
  }

  if(strcmp(argv[1], "get-primary") == 0) {
    uint8_t set_page[2];
    struct i2c_registers_type page1;

    set_page[0] = I2C_REGISTER_OFFSET_PAGE;
    set_page[1] = I2C_REGISTER_PAGE1;
    lock_i2c(fd);
    write_i2c(fd, set_page, sizeof(set_page));
    read_i2c(fd, &page1, sizeof(page1));
    unlock_i2c(fd);

    printf("primary channel = %u hz = %u\n", page1.primary_channel+1, page1.primary_channel_HZ);
  }
}

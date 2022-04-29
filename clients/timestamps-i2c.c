#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include "i2c.h"
#include "i2c_registers.h"
#include "timespec.h"
#include "adc_calc.h"
#include "vref_calc.h"
#include "tempcomp.h"

// 8+1+16+1 bits at 100khz = 260us to account for page request + i2c address transmit
#define REQUEST_LATENCY  0.000260
// assume response is immediate
#define RESPONSE_LATENCY 0.000000

struct rtt_ts {
   struct timespec i2c_start,i2c_end;
};

/*
static float calculate_offset(float diff_start, float diff_end, float rtt) {
  return diff_start + rtt/2.0;  
}
*/

static void get_i2c_tim(int fd, struct i2c_registers_type_page5 *page5, struct rtt_ts *rtt) {
  uint8_t set_page[2];
  ssize_t status_write;

  set_page[0] = I2C_REGISTER_OFFSET_PAGE;
  set_page[1] = I2C_REGISTER_PAGE5;
  lock_i2c(fd);

  clock_gettime(CLOCK_REALTIME, &rtt->i2c_start);
  status_write = write(fd, set_page, sizeof(set_page));
  clock_gettime(CLOCK_REALTIME, &rtt->i2c_end);

  if (status_write < 0) {
    perror("write to i2c failed");
  } else if (status_write != sizeof(set_page)) {
    fprintf(stderr, "short write %zu != %zu\n", status_write, sizeof(set_page));
  }

  read_i2c(fd, page5, sizeof(*page5));

  unlock_i2c(fd);
}

int main() {
  int fd;
  uint32_t last_ch2 = 0, basetime_tim2;
  struct timespec basetime = {0,0};
  struct timespec request_latency;
  struct tempcomp_data tempcomp_data;
  struct i2c_registers_type_page3 i2c_registers_page3;

  fd = open_i2c(I2C_ADDR); 

  get_i2c_page3(fd, &i2c_registers_page3, &tempcomp_data);

  double_to_timespec(&request_latency, REQUEST_LATENCY*1000000000.0);

  while(1) {
    struct i2c_registers_type_page5 page5;
    struct timespec i2c_rtt;
    struct rtt_ts rtt_ts;
    struct i2c_registers_type i2c_registers;
    struct i2c_registers_type_page2 i2c_registers_page2;

    get_i2c_tim(fd, &page5, &rtt_ts);
    if(page5.cur_tim2 == last_ch2) {
      fprintf(stderr,"ch2 unchanged count: %u->%u\n", page5.cur_tim2, last_ch2);
      sleep(1);
      continue;
    }
    add_timespecs(&rtt_ts.i2c_start, &request_latency);

    get_i2c_structs(fd, &i2c_registers, &i2c_registers_page2);
    add_adc_data(&i2c_registers, &i2c_registers_page2);

    if (basetime.tv_sec == 0) {
      basetime.tv_sec = rtt_ts.i2c_start.tv_sec;
      basetime.tv_nsec = rtt_ts.i2c_start.tv_nsec;
      basetime_tim2 = page5.cur_tim2;
    }

    uint32_t diff = page5.cur_tim2-basetime_tim2;
    double ppb = tempcomp(&tempcomp_data);
    double duration = (diff / (double)EXPECTED_FREQ) * (1 + ppb / 1000000000.0);
    double seconds;
    struct timespec diff_ts, ts_now, offset;

    diff_ts.tv_nsec = modf(duration, &seconds) * 1000000000.0;
    diff_ts.tv_sec = seconds;
    ts_now.tv_sec = basetime.tv_sec;
    ts_now.tv_nsec = basetime.tv_nsec;
    add_timespecs(&ts_now, &diff_ts);

    if (diff > EXPECTED_FREQ*30) {
      basetime_tim2 = page5.cur_tim2;
      basetime.tv_sec = ts_now.tv_sec;
      basetime.tv_nsec = ts_now.tv_nsec;
    }

    i2c_rtt.tv_sec = rtt_ts.i2c_end.tv_sec;
    i2c_rtt.tv_nsec = rtt_ts.i2c_end.tv_nsec;
    sub_timespecs(&i2c_rtt, &rtt_ts.i2c_start);

    offset.tv_sec = ts_now.tv_sec;
    offset.tv_nsec = ts_now.tv_nsec;
    sub_timespecs(&offset, &rtt_ts.i2c_start);

    printf("%u %u %.9f ", page5.cur_tim2, diff, duration);
    print_timespec(&rtt_ts.i2c_start);
    printf(" ");
    print_timespec(&ts_now);
    printf(" ");
    print_timespec(&i2c_rtt);
    printf(" ");
    print_timespec(&offset);
    printf(" %.3f %.3f %.3f %.1f\n", last_temp()*9/5.0+32.0, last_vref(), last_vbat(), tempcomp(&tempcomp_data));
    fflush(stdout);

    last_ch2 = page5.cur_tim2;

    sleep(1);
  }
}

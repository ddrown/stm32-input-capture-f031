#include <stdio.h>
#include <time.h>
#include <stdint.h>

#include "aging.h"

static float aging_b = 0;
static uint32_t aging_d = 0;

/* examples from two TCXOs:
 * b = -0.0000000163855  d = 1497219000
 * b = -0.00000000688527 d = 1493907000
 */

void read_tcxo_aging() {
  FILE *f;

  f = fopen("tcxo-aging","r");
  if(f == NULL) {
    return;
  }
  fscanf(f, "%f\n%u", &aging_b, &aging_d);
  fclose(f);
}

float calc_tcxo_aging() {
  float ppm = (time(NULL)-aging_d) * aging_b;
  return ppm;
}


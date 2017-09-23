#include <stdio.h>
#include <time.h>
#include <stdint.h>

#include "aging.h"

static float aging_b = 0;
static uint32_t aging_d = 0;

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


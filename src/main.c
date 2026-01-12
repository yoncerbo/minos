#include "common.h"

#include "hardware/init.c"

void kernel_main(void) {
  LOG("Hello World!\n");
  init_hardware();
}


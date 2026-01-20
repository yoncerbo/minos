#include "lib.h"

int global = 432;

int main(void) {
  sys_log("hello", 5);

  return global;
}


#include "cmn/lib.h"
#include "lib.h"

int main(void) {

  for (int i = 0; i < 10; ++i) {
    log("Program 2: %d", i);
    sys_yield();
  }
  return 0;
}


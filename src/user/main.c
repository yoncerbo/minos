#include "cmn/lib.h"
#include "lib.h"

int main(void) {

  // for (int i = 0; i < 10; ++i) {
    log("Iteration: %d", 1);
    sys_yield();
  // }
    log("Iteration: %d", 2);
    sys_yield();
  return 0;
}


#include "cmn/lib.h"
#include "lib.h"

int global = 432;

int main(void) {
  log("hello from the userspace: %d", global);
  return global;
}


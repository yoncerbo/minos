#include "cmn/lib.h"
#include "lib.h"

int global = 432;

int main(void) {
  log("hello world %d", 123);

  return global;
}


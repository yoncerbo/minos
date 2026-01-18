#include "common.h"
#include "arch.h"

NORETURN void user_main(void) {
  ASM("int3");

  sys_log("Hello, Userspace\n", -1);
  sys_exit(0);
}


#include "common.h"
#include "arch.h"

NORETURN void user_main(void) {
  sys_log("Hello, Userspace", -1);
  sys_exit(123);
}


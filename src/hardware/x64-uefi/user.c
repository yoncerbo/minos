#include "common.h"
#include "arch.h"

NORETURN void user_main(void) {
  // TODO: Why the exceptions aren't getting triggered
  // volatile size_t *value = (void *)0xFFFFFFFFDEADBEEF;
  // *value = 10;
  // ASM("int3");

  sys_log("Hello, Userspace\n", -1);
  sys_exit(0);
}


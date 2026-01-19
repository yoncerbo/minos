#include "cmn/lib.h"

SyscallError sys_log(const char *str, size_t limit) {
  size_t result = SYS_LOG;
  ASM("syscall" : "+a"(result) : "D"(str), "S"(limit));
  return result;
}

NORETURN void sys_exit(size_t error_code) {
  size_t result = SYS_EXIT;
  ASM("syscall" :: "a"(result), "D"(error_code));
  UNREACHABLE();
}

__attribute__((naked, section(".text.start")))
void _start(void) {
  sys_exit(main());
}

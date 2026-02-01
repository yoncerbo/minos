#include "cmn/lib.h"

SyscallError syscall0(SyscallType type) {
  size_t result = type;
  ASM("syscall" : "+a"(result) :: "r11", "rcx", "memory");
  return result;
}

SyscallError syscall1(SyscallType type, size_t a1) {
  size_t result = type;
  ASM("syscall" : "+a"(result) : "D"(a1) : "r11", "rcx", "memory");
  return result;
}

SyscallError syscall2(SyscallType type, size_t a1, size_t a2) {
  size_t result = type;
  ASM("syscall" : "+a"(result) : "D"(a1), "S"(a2) : "r11", "rcx", "memory");
  return result;
}

SyscallError sys_log(const char *str, size_t limit) {
  return syscall2(SYS_LOG, (size_t)str, limit);
}

SyscallError sys_yield(void) {
  return syscall0(SYS_YIELD);
}

NORETURN void sys_exit(size_t error_code) {
  syscall1(SYS_EXIT, error_code);
  UNREACHABLE();
}

Error sys_log_sink_write(void *this, const void *buffer, uint32_t limit) {
  sys_log(buffer, limit);
  return OK;
}

Sink SYS_LOG_SINK = {
  .write = sys_log_sink_write,
};


void _start(void) {
  LOG_SINK = &SYS_LOG_SINK;
  sys_exit(main());
}

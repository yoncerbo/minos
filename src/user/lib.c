#include "cmn/lib.h"

SyscallError sys_log(const char *str, size_t limit) {
  size_t result = SYS_WRITE;
  ASM("syscall" : "+a"(result) : "D"(str), "S"(limit));
  return result;
}

NORETURN void sys_exit(size_t error_code) {
  size_t result = SYS_EXIT;
  ASM("syscall" :: "a"(result), "D"(error_code));
  UNREACHABLE();
}

Error sys_log_sink_write(void *this, const void *buffer, uint32_t limit) {
  sys_log(buffer, limit);
}

Sink SYS_LOG_SINK = {
  .write = sys_log_sink_write,
};

__attribute__((naked, section(".text.start")))
void _start(void) {
  LOG_SINK = &SYS_LOG_SINK;
  sys_exit(main());
}

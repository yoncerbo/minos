#include "common.h"
#include "arch.h"

typedef struct PACKED {
  size_t rax;
  size_t rdi;
  size_t rsi;
  size_t rdx;
  size_t rcx; // user instruction pointer
  size_t r8;
  size_t r9;
  size_t r10;
  size_t r11; // flags
} SyscallFrame;

SyscallError sys_log(const char *str, size_t limit) {
  size_t result = SYS_LOG;
  ASM("syscall" : "+a"(result) : "D"(str), "S"(limit));
  return result;
}

NORETURN void sys_exit(size_t error_code) {
  size_t result = SYS_EXIT;
  ASM("syscall" : "+a"(result) : "D"(error_code));
  UNREACHABLE();
}

SYSV size_t handle_syscall(SyscallFrame *frame) {
  // TODO: Getting kernel thread context
  switch (frame->rax) {
    case SYS_LOG: {
      const char *str = (void *)frame->rdi;
      size_t limit = frame->rsi;
      // TODO: Proper argument checking
      if (str == NULL) {
        frame->rax = SYS_ERR_BAD_ARG;
        return 0;
      }
      for (size_t i = 0; i < limit && str[i]; ++i) {
        putchar(str[i]);
      }
    } break;
    case SYS_EXIT: {
      return 1;
    } break;
    default: {
      frame->rax = SYS_ERR_UNKNOWN_SYSCALL;
      return 0;
    } break;
  }

  frame->rax = SYS_OK; // return code
  return 0;
}


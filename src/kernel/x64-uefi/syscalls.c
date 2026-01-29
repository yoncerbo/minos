#include "cmn/lib.h"
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

#define MSR_GS_BAS 0xC0000101ull

SYSV size_t handle_syscall(SyscallFrame *frame) {
  size_t high, thread_context_addr;
  READ_MSR(MSR_GS_BAS, thread_context_addr, high);
  thread_context_addr |= high << 32;
  KernelThreadContext *ctx = (void *)thread_context_addr;

  switch (frame->rax) {
    case SYS_LOG: {
      const char *str = (void *)frame->rdi;
      size_t limit = frame->rsi;
      // TODO: Proper argument checking
      if (str == NULL) {
        frame->rax = SYS_ERR_BAD_ARG;
        return 0;
      }
      prints(ctx->user_log_sink, "[USER]%S", limit, str);
    } break;
    case SYS_EXIT: {
      frame->rax = frame->rdi;
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


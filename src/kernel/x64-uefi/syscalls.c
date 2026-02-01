#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

#define MSR_GS_BAS 0xC0000101ull

KernelThreadContext *get_thread_context(void) {
  size_t high, thread_context_addr;
  READ_MSR(MSR_GS_BAS, thread_context_addr, high);
  thread_context_addr |= high << 32;
  return (void *)thread_context_addr;
}

size_t handle_syscall(SyscallFrame *frame) {
  KernelThreadContext *ctx = get_thread_context();

  switch (frame->rax) {
    case SYS_LOG: {
      const char *str = (void *)frame->rdi;
      size_t limit = frame->rsi;
      // TODO: Proper argument checking
      if (str == NULL) {
        frame->rax = SYS_ERR_BAD_ARG;
        return 0;
      }
      prints(ctx->user_process->log_sink, "%S", limit, str);
    } break;
    case SYS_EXIT: {
      ctx->user_exit_code = frame->rdi;
      return 1;
    } break;
    case SYS_YIELD: {
      ctx->user_exit_code = frame->rdi;
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

#define CTX ((KernelThreadContext *)0)

__attribute__((naked))
void syscall_entry(void) {
  ASM("swapgs");
  ASM("mov gs:%0, rsp" :: "i"(&CTX->user_sp));
  ASM("mov rsp, gs:%0" :: "i"(&CTX->user_process));
  ASM("add rsp, %0" :: "i"(15 * 8));
  ASM("push r15\n push r14\n push r13\n push r12\n push rbp\n push rbx");
  ASM("push r11\n push r10\n push r9\n push r8\n push rcx\n"
      "push rdx\n push rsi\n push rdi\n push rax");
  ASM("mov rdi, rsp");
  ASM("mov rsp, gs:%0" :: "i"(&CTX->kernel_sp));
  ASM("call handle_syscall");
  ASM("test rax, rax");
  ASM("jnz exit_user_process");

  ASM("mov rsp, gs:%0" :: "i"(&CTX->user_process));
  ASM("pop rax\n pop rdi\n pop rsi\n pop rdx\n pop rcx\n"
      "pop r8\n pop r9\n pop r10\n pop r11");
  ASM("pop rbx\n pop rbp\n pop r12\n pop r13\n pop r14\n pop r15\n");
  ASM("mov rsp, gs:%0" :: "i"(&CTX->user_sp));
  ASM("swapgs\n sysretq");
}

#undef CTX

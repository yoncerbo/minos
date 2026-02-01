#include "cmn/lib.h"
#include "arch.h"
#include "common.h"

void load_user_process(Process *p, MemoryManager *kernel_mm, const char *elf_file) {
  paddr_t pml4_physical = alloc_pages2(kernel_mm->page_alloc, 1);
  PageTable *pml4 = (void *)(pml4_physical + kernel_mm->virtual_offset);
  memcpy(pml4, (void *)(kernel_mm->pml4 + kernel_mm->virtual_offset), sizeof(*pml4));

  p->mm = (MemoryManager){
    .page_alloc = kernel_mm->page_alloc,
    .start = PAGE_SIZE, // NOTE: Skip first null page
    .end = HIGHER_HALF,
    .objects_count = 1, // NOTE: first object is null
    .virtual_offset = kernel_mm->virtual_offset,
    .pml4 = pml4_physical,
  };
  flush_page_table(&p->mm);

  size_t program_entry;
  load_elf_file2(&p->mm, elf_file, &program_entry);

  // TODO: Create a protection for the stack
  uint8_t *stack = alloc(&p->mm, 8 * PAGE_SIZE);
  uint8_t *stack_top = stack + 8 * PAGE_SIZE - 1;
  p->sp = (vaddr_t)stack_top;

  p->frame.r11 = 0x0202;
  p->frame.rcx = program_entry;
}

#define CTX ((KernelThreadContext *)0)

// NOTE:
// System V calling convention:
// preserved by the function: rbx, rsp, rbp, r12, r13, r14, r15
// scratch registers: rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11

__attribute__((naked))
size_t run_user_process(void) {
  ASM("push rbx\n push rbp\n push r12\n push r13\n push r14\n push r15\n");
  ASM("mov gs:%0, rsp" :: "i"(&CTX->kernel_sp));
  ASM("mov rsp, gs:%0" :: "i"(&CTX->user_process));
  ASM("pop rax\n pop rdi\n pop rsi\n pop rdx\n pop rcx\n"
      "pop r8\n pop r9\n pop r10\n pop r11");
  ASM("mov rsp, gs:%0" :: "i"(&CTX->user_sp));
  ASM("swapgs\n sysretq");
}

__attribute__((naked))
void exit_user_process(void) {
  ASM("mov rax, gs:%0" :: "i"(&CTX->user_exit_code));
  ASM("pop r15\n pop r14\n pop r13\n pop r12\n pop rbp\n pop rbx\n ret");
}

#undef CTX

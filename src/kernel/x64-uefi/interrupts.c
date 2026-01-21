#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

void set_idt_descriptor(InterruptDescriptor *idt, uint8_t descriptor_index, size_t handler, uint8_t flags) {
  // SOURCE: https://wiki.osdev.org/Interrupt_Descriptor_Table#Gate_Descriptor_2
  InterruptDescriptor *desc = &idt[descriptor_index];
  *desc = (InterruptDescriptor){
    .isr0_15 = handler & 0xFFFF,
    .gdt_selector = GDT_KERNEL_CODE * 8, // kernel code selector
    .type_attributes = flags,
    .isr16_31 = (handler >> 16) & 0xFFFF,
    .isr32_63 = handler >> 32,
    .reserved = 0,
    .interrupt_stack_table = 01,
  };
}


// SOURCE: AMD Volume 2: 8.4.2
typedef enum {
  PAGE_FAULT_PROTECTION_VIOLATION = 1 << 0,
  PAGE_FAULT_CAUSED_BY_WRITE      = 1 << 1,
  PAGE_FAULT_USER_MODE            = 1 << 2,
  PAGE_FAULT_MALFORMED_TABLE      = 1 << 3,
  PAGE_FAULT_INSTRUCTION_FETCH    = 1 << 4,
} PageFaultError;

extern char ISR_VECTORS[];

void setup_idt(InterruptDescriptor *idt) {
  uint8_t flags = 0xEE;
  size_t vectors = (size_t)ISR_VECTORS;

  for (size_t i = 0; i < 256; ++i) {
    set_idt_descriptor(idt, i, vectors + i * 16, flags);
  }

  IdtPtr idt_ptr = { sizeof(*idt) * 256 - 1, idt };
  ASM("lidt %0" : : "m"(idt_ptr));
}

typedef struct {
  size_t rax; size_t rbx; size_t rcx;
  size_t rdx; size_t rsi; size_t rdi;
  size_t rbp; size_t r8; size_t r9;
  size_t r10; size_t r11; size_t r12;
  size_t r13; size_t r14; size_t r15;

  size_t vector_number, error_code;
  size_t ip, cs, flags, sp, ss;
} IsrFrame;

typedef enum {
  INT_BREAKPOINT = 3,
  INT_INVALID_OPCODE = 6,
  INT_DOUBLE_FAULT = 8,
  INT_GENERAL_PROTECTION = 13,
  INT_PAGE_FAULT = 14,
  INT_SYSCALL = 0x80,
} InterruptVector;

SYSV IsrFrame *interrupt_handler(IsrFrame *frame) {
  switch (frame->vector_number) {
    case INT_PAGE_FAULT: {
      size_t cr2;
      ASM("mov %0, cr2" : "=r"(cr2));
      log("Page fault, cr2=%X", cr2);
    } break;
    default: {
      log("An interrupt occured: vector=%d", frame->vector_number);
    } break;
  }
  log("  error_code=%d", frame->error_code);
  log("  instruction_pointer=0x%x", frame->ip);
  log("  stack_pointer=0x%x", frame->sp);
  log("  flags=0x%x", frame->flags);
  log("  cs=0x%x, ss=0x%x", frame->cs, frame->ss);
  for(;;) WFI();
  UNREACHABLE();
}

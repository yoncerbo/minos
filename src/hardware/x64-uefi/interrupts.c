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

// SOURCE: https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-interrupt-function-attribute_002c-x86

__attribute__((interrupt))
void sysstem_call_handler(InterruptFrame *frame){
  printf("System call interrupt\n");
  for (;;) WFI();
}

__attribute__((interrupt))
void breakpoint_handler(InterruptFrame *frame){
  printf("Breakpoint interrupt\n");
  for (;;) WFI();
}

__attribute__((interrupt))
void double_fault_handler(InterruptFrame *frame, size_t error_code) {
  printf("Double fault: error_code=%d\n", error_code);
  printf("  instruction_pointer=0x%X\n", frame->ip);
  printf("  stack_pointer=0x%X\n", frame->sp);
  printf("  flags=0x%X\n", frame->flags);
  printf("  cs=0x%x, ss=0x%x", frame->cs, frame->ss);
  for (;;) WFI();
}

__attribute__((interrupt))
void general_protection_handler(InterruptFrame *frame, size_t error_code) {
  printf("General protection fault: error_code=%d\n", error_code);
  printf("  instruction_pointer=0x%X\n", frame->ip);
  printf("  stack_pointer=0x%X\n", frame->sp);
  printf("  flags=0x%X\n", frame->flags);
  printf("  cs=0x%x, ss=0x%x", frame->cs, frame->ss);
  for (;;) WFI();
}

// SOURCE: AMD Volume 2: 8.4.2
typedef enum {
  PAGE_FAULT_PROTECTION_VIOLATION = 1 << 0,
  PAGE_FAULT_CAUSED_BY_WRITE      = 1 << 1,
  PAGE_FAULT_USER_MODE            = 1 << 2,
  PAGE_FAULT_MALFORMED_TABLE      = 1 << 3,
  PAGE_FAULT_INSTRUCTION_FETCH    = 1 << 4,
} PageFaultError;

__attribute__((interrupt))
void page_fault_handler(InterruptFrame *frame, size_t error_code) {
  size_t cr2;
  ASM("mov %0, cr2" : "=r"(cr2));

  printf("Page fault: error_code=%d\n", error_code);
  printf("  address=0x%X\n", cr2);
  printf("  instruction_pointer=0x%X\n", frame->ip);
  printf("  stack_pointer=0x%X\n", frame->sp);
  printf("  flags=0x%X\n", frame->flags);
  printf("  cs=0x%x, ss=0x%x", frame->cs, frame->ss);
  for (;;) WFI();
}

void setup_idt(InterruptDescriptor *idt) {
  uint8_t flags = 0xEE;
  set_idt_descriptor(idt, 3, (size_t)breakpoint_handler, 0xEE);
  set_idt_descriptor(idt, 8, (size_t)double_fault_handler, 0xEE);
  set_idt_descriptor(idt, 13, (size_t)general_protection_handler, 0xEE);
  set_idt_descriptor(idt, 14, (size_t)page_fault_handler, 0xEE);
  set_idt_descriptor(idt, 0x80, (size_t)sysstem_call_handler, 0xEE);

  IdtPtr idt_ptr = { sizeof(*idt) * 256 - 1, idt };
  ASM("lidt %0" : : "m"(idt_ptr));
}

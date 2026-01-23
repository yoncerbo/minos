#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

#include "memory.c"
#include "interrupts.c"
#include "gdt.c"
#include "syscalls.c"
#include "logging.c"
#include "drawing.c"
#include "elf.c"

INCLUDE_ASM("utils.s");

Tss TSS;
GdtEntry GDT[GDT_COUNT];
ALIGNED(16) InterruptDescriptor IDT[256];

uint8_t INTERUPT_STACK[PAGE_SIZE];
uint8_t USER_STACK[4 * PAGE_SIZE];

void _start(BootData *data) {
  LOG_SINK = &QEMU_DEBUGCON_SINK;

  setup_gdt_and_tss((GdtEntry *)GDT, &TSS, &INTERUPT_STACK[sizeof(INTERUPT_STACK) - 1]);
  setup_idt(IDT);

  fill_surface(&data->fb, BLACK);
  FB_SINK.surface = data->fb;
  FB_SINK.font = data->font;
  LOG_SINK = &FB_SINK.sink;

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  vaddr_t user_entry;
  load_elf_file(&data->alloc, data->pml4, data->user_efi_file, &user_entry);

  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));

  log("Running user program");
  size_t status;
  status = run_user_program((void *)user_entry, &USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);

  log("OK");

  for(;;) WFI();
}

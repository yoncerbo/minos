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
#include "apic.c"

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

  uint32_t lapic_id = setup_apic(&data->alloc, data->pml4);
  DEBUGD(lapic_id);

  map_page(&data->alloc, data->pml4, data->io_apic_addr, data->io_apic_addr,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE);
  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));
  uint32_t ioapic_ver = read_ioapic_register(data->io_apic_addr, IO_APIC_VER);
  size_t input_count = ((ioapic_ver >> 16) & 0xFF) + 1;
  DEBUGD(input_count);

  uint32_t keyboard_reg = 0x10 + 2;
  write_ioapic_register(data->io_apic_addr, keyboard_reg, 0xF1);
  write_ioapic_register(data->io_apic_addr, keyboard_reg + 1, lapic_id >> 56);

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  vaddr_t user_entry;
  load_elf_file(&data->alloc, data->pml4, data->user_efi_file, &user_entry);

  paddr_t user_stack = alloc_pages2(&data->alloc, 4);
  paddr_t user_stack_top = user_stack + 4 * PAGE_SIZE - 1;

  map_pages(&data->alloc, data->pml4, user_stack, user_stack, PAGE_SIZE * 4,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);

  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));

  log("Running user program");
  size_t status;
  status = run_user_program((void *)user_entry, (void *)user_stack_top);
  DEBUGD(status);

  log("OK");

  ASM("sti");

  for(;;) {
    WFI();
  }
}

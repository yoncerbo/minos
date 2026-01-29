#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

#include "interfaces/input.h"
#include "memory.c"
#include "interrupts.c"
#include "gdt.c"
#include "syscalls.c"
#include "drawing.c"
#include "elf.c"
#include "apic.c"
#include "pci.c"
#include "text_input.c"
#include "console.c"

INCLUDE_ASM("utils.s");

Tss TSS;
GdtEntry GDT[GDT_COUNT];
ALIGNED(16) InterruptDescriptor IDT[256];

uint8_t INTERUPT_STACK[PAGE_SIZE];
uint8_t USER_STACK[4 * PAGE_SIZE];

uint8_t SCANCODE_TO_KEY[256] = {
  [1] = KEY_ESC,
};

void _start(BootData *data) {
  Console console = {
    .sink.write = console_write,
    .font = data->font,
    .surface = data->fb,
    .line_spacing = 4,
    .bg = 0x11111111,
    .fg = WHITE,
  };
  clear_console(&console);
  LOG_SINK = &console.sink;

  setup_gdt_and_tss((GdtEntry *)GDT, &TSS, &INTERUPT_STACK[sizeof(INTERUPT_STACK) - 1]);
  setup_idt(IDT);

  uint32_t lapic_id = setup_apic(&data->alloc, data->pml4);
  DEBUGD(lapic_id);

  discover_pci_devices(&data->alloc, data->pml4);

  map_page(&data->alloc, data->pml4, data->io_apic_addr, data->io_apic_addr,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE);
  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));
  uint32_t ioapic_ver = read_ioapic_register(data->io_apic_addr, IO_APIC_VER);
  size_t input_count = ((ioapic_ver >> 16) & 0xFF) + 1;
  DEBUGD(input_count);

  uint32_t keyboard_reg = 0x10 + 2;
  write_ioapic_register(data->io_apic_addr, keyboard_reg, 0xF1);
  write_ioapic_register(data->io_apic_addr, keyboard_reg + 1, (size_t)lapic_id >> 56);

  KernelThreadContext thread_context = {
    .user_log_sink = &console.sink,
  };
  enable_system_calls(&thread_context);

  vaddr_t user_entry;
  load_elf_file(&data->alloc, data->pml4, data->user_efi_file, &user_entry);

  paddr_t user_stack = alloc_pages2(&data->alloc, 4);
  paddr_t user_stack_top = user_stack + 4 * PAGE_SIZE - 1;

  map_pages(&data->alloc, data->pml4, user_stack, user_stack, PAGE_SIZE * 4,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);

  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));

  log("Running user program");
  console.fg = 0x00ff00ff;
  size_t status;
  status = run_user_program((void *)user_entry, (void *)user_stack_top);
  DEBUGD(status);
  console.fg = WHITE;

  ASM("sti");

  uint32_t scancode_processed = 0;

  draw_console_prompt(&console);

  // SOURCE: https://wiki.osdev.org/PS/2_Keyboard
  for(;;) {
    WFI();
    uint32_t diff = SCANCODE_POSITION - scancode_processed;
    for (uint32_t i = 0; i < diff; ++i) {
      uint8_t scancode = SCANCODE_BUFFER[scancode_processed++ % SCANCODE_BUFFER_SIZE];
      // TODO: Support more scancodes
      // TODO: Handling keys with multiple scancodes
      uint32_t released = scancode >> 7;
      scancode &= 128 - 1;
      if (!scancode || scancode > KEY_F12) continue;

      InputEvent event = {
        .type = EV_KEY,
        .code = scancode,
        .value = released ? 0 : 1,
      };

      if (!push_console_input_event(&console, event)) continue;

      uint32_t len;
      const char *cmd = strip_string(console.command_buffer, console.buffer_pos, &len);
      if (len == 4 && are_strings_equal(cmd, "ping", 4)) {
        prints(&console.sink, "pong\n");
      } else {
        prints(&console.sink, "Unknown command: '%S'\n", len, cmd);
      }
      console.buffer_pos = 0;
      draw_console_prompt(&console);
    }
  }
}

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
#include "logging.c"
#include "process.c"

INCLUDE_ASM("utils.s");

extern char FONT_FILE[], USER_FILE1[], USER_FILE2[];
__asm__("FONT_FILE: .incbin \"res/font1.psf\"");
__asm__("USER_FILE1: .incbin \"out/x64-uefi/user_main1.elf\"");
__asm__("USER_FILE2: .incbin \"out/x64-uefi/user_main2.elf\"");

Tss TSS;
GdtEntry GDT[GDT_COUNT];
ALIGNED(16) InterruptDescriptor IDT[256];

ALIGNED(PAGE_SIZE) uint8_t INTERUPT_STACK[PAGE_SIZE];

#define MAX_PHYSICAL_RANGES 256
PhysicalPageRange PAGE_RANGES[MAX_PHYSICAL_RANGES];

typedef struct {
  Sink sink;
  Console *console;
  uint32_t fg;
  uint32_t bg;
} ColoredConsoleSink;

Error colored_write(void *data, const void *buffer, uint32_t limit) {
  ColoredConsoleSink *this = data;

  uint32_t fg = this->console->fg;
  uint32_t bg = this->console->bg;
  this->console->fg = this->fg;
  this->console->bg = this->bg;
  Error err = write(&this->console->sink, buffer, limit);
  this->console->fg = fg;
  this->console->bg = bg;
  return err;
}

void _start(BootData *data) {
  LOG_SINK = &QEMU_DEBUGCON_SINK;

  KernelThreadContext ctx = {0};

  // TODO: Allocate interrupt stack per thread
  uint8_t *int_stack_end = &INTERUPT_STACK[sizeof(INTERUPT_STACK) - 8];
  *(uint64_t *)int_stack_end = (size_t)&ctx;
  
  setup_gdt_and_tss((GdtEntry *)GDT, &TSS, int_stack_end);
  setup_idt(IDT);

  ASSERT(data->ranges_len <= MAX_PHYSICAL_RANGES);
  memcpy(PAGE_RANGES, data->ranges, data->ranges_len * sizeof(PAGE_RANGES[0]));

  PageAllocator2 page_alloc = {
    .ranges = PAGE_RANGES,
    .capacity = MAX_PHYSICAL_RANGES,
    .len = data->ranges_len,
  };

  // NOTE:
  // Virtual memory maps from the bootloader:
  // physical memmory ofseted by HIGHER_HALF
  // kernel code and data at -2GiB
  // kernel stack right below the kernel base
  // boot data above HIGHER_HALF
  // bootloader image code and data - to reuse

  // TODO: Map all physical memory

  MemoryManager mm = {
    .page_alloc = &page_alloc,
    // NOTE: Offset for physical memory mapping
    .start = HIGHER_HALF + 128ull * 1024 * 1024 * 1024,
    .end = KERNEL_BASE - 8 * PAGE_SIZE,
    .first_object = 0,
    .objects_count = 1, // NOTE: first object is null
    .pml4 = data->pml4,
    .virtual_offset = HIGHER_HALF,
  };

  data->fb.ptr = (void *)alloc_physical(&mm, (paddr_t)data->fb.ptr, data->fb.pitch * data->fb.height,
      PAGE_BIT_WRITABLE | PAGE_BIT_PRESENT);

  // Unmap the bootloader image
  // map_pages2(&mm, data->bootloader_image_base, 0, data->bootloader_image_size, 0);

  memset((void *)(data->pml4 + mm.virtual_offset), 0, 128 * 8);

  flush_page_table(&mm);
  log("Starting kernel");

  Console console = {
    .sink.write = console_write,
    .surface = data->fb,
    .line_spacing = 4,
    .bg = 0x11111111,
    .fg = WHITE,
  };
  load_psf2_font(&console.font, FONT_FILE);
  clear_console(&console);

  ColoredConsoleSink kernel_sink = {
    .sink.write = colored_write,
    .console = &console,
    .fg = WHITE,
    .bg = 0x11111111,
  };
  LOG_SINK = &kernel_sink.sink;

  setup_apic(&mm, &APIC);
  DEBUGD(APIC.id);

  discover_pci_devices(&mm);

  vaddr_t io_apic = alloc_physical(&mm, data->io_apic_addr, PAGE_SIZE,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE);
  flush_page_table(&mm);

  uint32_t ioapic_ver = read_ioapic_register(io_apic, IO_APIC_VER);
  size_t input_count = ((ioapic_ver >> 16) & 0xFF) + 1;
  DEBUGD(input_count);

  uint32_t keyboard_reg = 0x10 + 2;
  write_ioapic_register(io_apic, keyboard_reg, 0xF1);
  write_ioapic_register(io_apic, keyboard_reg + 1, (size_t)APIC.id >> 56);

  enable_system_calls(&ctx);
  ColoredConsoleSink user_sink1 = {
    .sink.write = colored_write,
    .console = &console,
    .fg = 0x00ff00ff,
    .bg = 0x11111111,
  };
  ColoredConsoleSink user_sink2 = {
    .sink.write = colored_write,
    .console = &console,
    .fg = 0x00ff0000,
    .bg = 0x11111111,
  };

  Process p1;
  load_user_process(&p1, &mm, USER_FILE1);
  p1.log_sink = &user_sink1.sink;
  Process p2;
  load_user_process(&p2, &mm, USER_FILE2);
  p2.log_sink = &user_sink2.sink;

  for (int i = 0; i < 5; ++i) {
    run_user_process(&ctx, &p1);
    run_user_process(&ctx, &p2);
    log("Back in kernel");
  }

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

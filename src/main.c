
#include "common.c"
#include "hardware.h"
#include "memory.h"
#include "uart.c"
#include "print.c"
#include "memory.c"
#include "plic.c"
#include "interrupts.c"
#include "virtio/virtio.c"
#include "virtio/virtio_blk.c"
#include "virtio/virtio_net.c"
#include "virtio/virtio_gpu.c"
#include "virtio/virtio_input.c"
#include "fs/tar.c"
#include "fs/fat.c"
#include "fs/vfs.c"
#include "networking.c"
#include "virtio_input.h"
#include "drawing.c"

// TODO: setup proper memory handling and allocators
// TODO: task system, processes
// TODO: virtio gpu and input

void kernel_main(void) {
  memset(BSS_START, 0, BSS_END - BSS_START);

  LOG("Starting kernel...\n");

  uint32_t flags, *page_table = (void *)alloc_pages(1);
  LOG("Mapping physical pages into virtual addresses, table at %x\n", page_table);

  flags = PAGE_R | PAGE_W | PAGE_X;
  LOG("|> kernel pages from=0x%x, to=0x%x\n", KERNEL_BASE, HEAP_END);
  for (char *paddr = KERNEL_BASE; paddr < HEAP_END; paddr += PAGE_SIZE) {
    map_page(page_table, (paddr_t)paddr, (vaddr_t)paddr, flags);
  }

  flags = PAGE_R | PAGE_W;
  uint32_t MMIO_START = 0x02004000; // PLIC
  uint32_t MMIO_END = 0x10100000;
  LOG("|> mmio pages from=0x%x, to=0x%x\n", MMIO_START, MMIO_END); 
  for (char *paddr = (char *)MMIO_START; paddr < (char *)MMIO_END; paddr += PAGE_SIZE) {
    map_page(page_table, (paddr_t)paddr, (vaddr_t)paddr, flags);
  }

  LOG("Setting up virtual memory\n");
  __asm__ __volatile__(
      "sfence.vma\n"
      "csrw satp, %[satp]\n"
      "sfence.vma\n"
      :
      : [satp] "r" (SATP_SV32 | ((uint32_t)page_table / PAGE_SIZE))
  );

  uart_init();

  plic_enablep(UART_INT, 3);
  plic_enablep(VIRTIO_KEYBOARD_INT, 3);
  plic_enablep(VIRTIO_MOUSE_INT, 3);
  plic_set_threshold(0);

  // sbi_set_timer(0);

  VirtioInput keyboard = virtio_input_init(VIRTIO_KEYBOARD);
  VirtioInput mouse = virtio_input_init(VIRTIO_MOUSE);

  VirtioGpu gpu = virtio_gpu_init(VIRTIO_GPU);
  for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
    gpu.fb[i] = bswap32(0x333333ff);
  }

  Str str = STR("Hello, World!");
  draw_chars(gpu.fb, 50, 50, WHITE, str.ptr, str.len);
  draw_chars(gpu.fb, 50, 62, WHITE, "Some more strings", 50);

  virtio_gpu_flush(&gpu);

  LOG("Initialization finished\n");
  for (;;) {
    VirtioInputEvent ev;
    // TODO: is it interrupt-proof enough?
    if (is_keyboard) {
      is_keyboard = false;
      while (virtio_input_get(&keyboard, &ev)) {
        DEBUGD(ev.type);
        DEBUGD(ev.value);
        DEBUGD(ev.code);
        putchar(10);
      }
    }
    if (is_mouse) {
      is_mouse = false;
      while (virtio_input_get(&mouse, &ev)) {
        DEBUGD(ev.type);
        DEBUGD(ev.value);
        DEBUGD(ev.code);
        putchar(10);
      }
    }
    __asm__ __volatile__("wfi");
  }
}

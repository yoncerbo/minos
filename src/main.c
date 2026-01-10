
#include "common.c"
#include "common.h"
#include "hardware.h"
#include "memory.h"
#include "plic.h"
#include "uart.c"
#include "print.c"
#include "memory.c"
#include "plic.c"
#include "interrupts.c"
#include "virtio.h"
#include "virtio/virtio.c"
#include "virtio/virtio_blk.c"
#include "virtio/virtio_net.c"
#include "virtio/virtio_gpu.c"
#include "virtio/virtio_input.c"
#include "fs/tar.c"
#include "fs/fat.c"
#include "fs/vfs.c"
#include "networking.c"
#include "virtio_blk.h"
#include "virtio_gpu.h"
#include "virtio_input.h"
#include "drawing.c"

// TODO: setup proper memory handling and allocators
// TODO: task system, processes
// TODO: virtio gpu and input

void kernel_main(void) {
  memset(BSS_START, 0, BSS_END - BSS_START);
  uart_init();

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

  plic_enablep(UART_INT, 3);

#define MAX_INPUT_DEVICES 8
  static VirtioInput input_devices[MAX_INPUT_DEVICES] = {0};
  uint32_t input_devices_len = 0;

  VirtioGpu gpu = {0};

  // TODO: Automatic virtio device detection and handling
  for (uint32_t i = 0; i < VIRTIO_COUNT; ++i) {
    uint32_t dev_addr = VIRTIO_MMIO_START + i * 0x1000;
    VirtioDevice *dev = (void *)dev_addr;
    if (dev->magic != VIRTIO_MAGIC) continue;
    if (dev->version != 1) {
      LOG("Virtio device with uknown version %d, at address 0x%x\n", dev->version, dev_addr);
      continue;
    }
    switch (dev->device) {
      case VIRTIO_DEVICE_NONE: {
        continue;
      } break;
      case VIRTIO_DEVICE_INPUT: {
        // TODO: Quering the device information
        ASSERT(input_devices_len < MAX_INPUT_DEVICES);
        input_devices[input_devices_len++] = virtio_input_init(dev);
        plic_enablep(VIRTIO_INTERRUPT_START + i, 3);
      } break;
      case VIRTIO_DEVICE_GPU: {
        ASSERT(gpu.dev == NULL);
        gpu = virtio_gpu_init(dev);
        plic_enablep(VIRTIO_INTERRUPT_START + i, 3);
      } break;
      default: {
        LOG("Unknown virtio device type %d, at address 0x%x\n", dev->device, dev_addr);
      } break;
    }
  }
  plic_set_threshold(0);

  // sbi_set_timer(0);

  if (gpu.dev) {
    for (uint32_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; ++i) {
      gpu.fb[i] = bswap32(0x333333ff);
    }

    Str str = STR("Hello, World!");
    draw_chars(gpu.fb, 50, 50, WHITE, str.ptr, str.len);
    draw_chars(gpu.fb, 50, 62, WHITE, "Some more strings", 50);

    virtio_gpu_flush(&gpu);
  }

  LOG("Initialization finished\n");
  for (;;) {
    for (uint32_t i = 0; i < input_devices_len; ++i) {
      VirtioInputEvent event;
      while (virtio_input_get(&input_devices[i], &event)) {
        // TODO: Input handling
      }
    }
    __asm__ __volatile__("wfi");
  }
}

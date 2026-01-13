
#include "../common.c"
#include "common.h"
#include "uart.c"
#include "../print.c"
#include "plic.c"
#include "interrupts.c"
#include "virtio.h"
#include "virtio.c"
#include "virtio_blk.c"
#include "virtio_net.c"
#include "virtio_gpu.c"
#include "virtio_input.c"
#include "../fs/tar.c"
#include "../fs/fat.c"
#include "../fs/vfs.c"
#include "../networking.c"
#include "memory.c"

#include "kernel.c"

// TODO: setup proper memory handling and allocators
// TODO: task system, processes
// TODO: virtio gpu and input

void kernel_main(out Hardware **hardware) {
  memset(BSS_START, 0, BSS_END - BSS_START);
  uart_init();

  LOG("Starting kernel...\n", 0);

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

  LOG("Setting up virtual memory\n", 0);
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

#define MAX_BLK_DEVICES 8
  static VirtioBlkdev blk_devices[MAX_BLK_DEVICES] = {0};
  uint32_t blk_devices_len = 0;

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
        if (input_devices_len >= MAX_INPUT_DEVICES) {
          LOG("Not enough slots for virtio blokdev, len=%d", input_devices_len);
          break;
        }
        input_devices[input_devices_len++] = virtio_input_init(dev);
        plic_enablep(VIRTIO_INTERRUPT_START + i, 3);
        LOG("Connected virtio_input at address 0x%x\n", dev_addr);
      } break;
      case VIRTIO_DEVICE_GPU: {
        ASSERT(gpu.dev == NULL);
        gpu = virtio_gpu_init(dev);
        // plic_enablep(VIRTIO_INTERRUPT_START + i, 3);
        LOG("Connected virtio_gpu at address 0x%x\n", dev_addr);
      } break;
      case VIRTIO_DEVICE_BLK: {
        if (blk_devices_len >= MAX_BLK_DEVICES) {
          LOG("Not enough slots for virtio blokdev, len=%d", blk_devices_len);
          break;
        }
        blk_devices[blk_devices_len++] = virtio_blk_init(dev);
        // plic_enablep(VIRTIO_INTERRUPT_START + i, 3);
        LOG("Connected virito_blkdev at address 0x%x\n", dev_addr);
      } break;
      default: {
        LOG("Unknown virtio device type %d, at address 0x%x\n", dev->device, dev_addr);
      } break;
    }
  }
  plic_set_threshold(0);

  Hardware hw = {
    .gpu = &gpu,
    .blk_devices = blk_devices,
    .blk_devices_count = blk_devices_len,
    .input_devices = input_devices,
    .input_devices_count = input_devices_len,
  };

  // sbi_set_timer(0);

  LOG("Initialization finished\n", 0);
  kernel_init(&hw);
  for (;;) {
    __asm__ __volatile__("wfi");
    kernel_update(&hw);
  }
}

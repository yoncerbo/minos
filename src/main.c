
#include "common.h"
#include "hardware.h"
#include "uart.c"
#include "print.c"
#include "memory.c"
#include "plic.c"
#include "interrupts.c"
#include "virtio/virtio.c"
#include "virtio/virtio_blk.c"
#include "virtio/virtio_net.c"
#include "fs/tar.c"
#include "fs/fat.c"
#include "fs/vfs.c"
#include "networking.c"

// TODO: clean up common.c
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
  plic_enablep(VIRTIO_NET_INT, 3);
  plic_set_threshold(0);

  // sbi_set_timer(0);

  LOG("Initialization finished\n");
  for (;;) __asm__ __volatile__("wfi");
}

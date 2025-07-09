
#include "common.h"
#include "print.c"
#include "memory.c"
#include "interrupts.c"

void kernel_main(void) {
  LOG("Starting kernel...\n");

  uint32_t flags, *page_table = alloc_pages(1).inner;
  LOG("Mapping physical pages into virtual addresses, table at %x\n", page_table);

  flags = PAGE_R | PAGE_W | PAGE_X;
  LOG("|> kernel pages from=0x%x, to=0x%x\n", KERNEL_BASE, HEAP_END);
  for (char *paddr = KERNEL_BASE; paddr < HEAP_END; paddr += PAGE_SIZE) {
    map_page(page_table, (vaddr_t){ paddr }, (paddr_t){ paddr }, flags);
  }

  flags = PAGE_R | PAGE_W;
  uint32_t MMIO_START = 0x10000000; // UART
  uint32_t MMIO_END = 0x10000007; // UART_END
  LOG("|> mmio pages from=0x%x, to=0x%x\n", MMIO_START, MMIO_END); 
  for (char *paddr = (char *)MMIO_START; paddr < (char *)MMIO_END; paddr += PAGE_SIZE) {
    map_page(page_table, (vaddr_t){ paddr }, (paddr_t){ paddr }, flags);
  }

  LOG("Setting up virtual memory\n");
  __asm__ __volatile__(
      "sfence.vma\n"
      "csrw satp, %[satp]\n"
      "sfence.vma\n"
      :
      : [satp] "r" (SATP_SV32 | ((uint32_t)page_table / PAGE_SIZE))
  );

  LOG("Initialization finished\n");
  for (;;) __asm__ __volatile__("wfi");
}


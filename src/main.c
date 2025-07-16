
#include "common.h"
#include "uart.c"
#include "sbi.c"
#include "print.c"
#include "memory.c"
#include "interrupts.c"
#include "plic.c"
#include "virtio.c"

void kernel_main(void) {
  // TOOD: zero the bss section

  // TODO: implement proper testing in qemu,
  // then move it there and add other stuff
  ASSERT(sizeof(uint64_t) == 8);
  ASSERT(sizeof(int64_t) == 8);
  ASSERT(sizeof(uint32_t) == 4);
  ASSERT(sizeof(int32_t) == 4);
  ASSERT(sizeof(uint16_t) == 2);
  ASSERT(sizeof(int16_t) == 2);
  ASSERT(sizeof(uint8_t) == 1);
  ASSERT(sizeof(int8_t) == 1);

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

  plic_set_threshold(0);
  plic_enable(10);
  plic_set_priority(10, 3);
  plic_enable(1);
  plic_set_priority(1, 3);

  //test_virtio_blkdev();

  // sbi_set_timer(0);

  test_virtio();

  LOG("Initialization finished\n");
  for (;;) __asm__ __volatile__("wfi");
}


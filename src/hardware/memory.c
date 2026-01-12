#include "common.h"

// Page table entry
// 32-10 Page number
// 9-8 For use
// 7 Dirty
// 6 Accessed
// 5 Gobal
// 4 User
// 3 Executable
// 2 Writeable
// 1 Readable
// 0 Valid

// Virtual address
// 31-22 Level 1 index
// 21-12 Level 0 index
// 11-0 Offset

void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
  uint32_t va = (uint32_t)vaddr;
  uint32_t pa = (uint32_t)paddr;
  if (va & 0x7) PANIC("unaligned vaddr %x\n", vaddr);
  if (pa & 0x7) PANIC("unaligned paddr %x\n", paddr);

  uint32_t vpn1 = (va >> 22) & 0x3ff;
  if ((table1[vpn1] & PAGE_V) == 0) {
    uint32_t pt_paddr = (uint32_t)alloc_pages(1);
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
  }

  uint32_t vpn0 = (va >> 12) & 0x3ff;
  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  table0[vpn0] = ((pa / PAGE_SIZE) << 10) | flags | PAGE_V;
}

uint32_t *page_entry(uint32_t *table1, vaddr_t page) {
  ASSERT((page & 0x7) == 0);

  uint32_t vpn1 = (page >> 22) & 0x3ff;
  uint32_t vpn0 = (page >> 12) & 0x3ff;

  ASSERT(table1[vpn1] & PAGE_V);

  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  return &table0[vpn0];
}

paddr_t alloc_pages(uint32_t count) {
  static paddr_t next_paddr = (size_t)HEAP_START;
  paddr_t paddr = next_paddr;
  next_paddr += count * PAGE_SIZE;

  if (next_paddr > (size_t)HEAP_END) {
    PANIC("Failed to allocate %d pages: out of memory!\n", count);
  }

  // TODO: zero the memory
  return paddr;
}


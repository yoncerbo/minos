#include "common.h"
#include "arch.h"

// Returns zeroed page
paddr_t alloc_pages(PageAllocator *allocator, size_t page_count) {
  ASSERT(allocator->page_offset + page_count < allocator->page_count);
  paddr_t addr = allocator->start + allocator->page_offset * PAGE_SIZE;
  memset((void *)addr, 0, PAGE_SIZE * page_count);
  allocator->page_offset += page_count;
  return addr;
}

void map_page_identity(PageAllocator *alloc, PageTable *level4_table, size_t addr, size_t flags) {
  ASSERT(!(addr % PAGE_SIZE));

  uint32_t page_table_indices[4] = {
    (addr >> 39) % 512,
    (addr >> 30) % 512,
    (addr >> 21) % 512,
    (addr >> 12) % 512,
  };

  flags |= PAGE_BIT_PRESENT;

  PageTable *page_table = level4_table;
  for (int i = 0; i < 3; ++i) {
    uint32_t index = page_table_indices[i];
    size_t entry = page_table->entries[index];
    if (!(entry & PAGE_BIT_PRESENT)) {
      size_t table_addr = alloc_pages(alloc, 1);
      memset((void *)table_addr, 0, PAGE_SIZE);
      page_table->entries[index] = (table_addr & PAGE_ADDR_MASK) | flags;
      map_page_identity(alloc, level4_table, table_addr, PAGE_BIT_WRITABLE | PAGE_BIT_USER);
    }
    page_table = (void *)(page_table->entries[index] & PAGE_ADDR_MASK);
  }

  size_t entry = page_table->entries[page_table_indices[3]];
  if (!(entry & PAGE_BIT_PRESENT)) {
    page_table->entries[page_table_indices[3]] = (addr & PAGE_ADDR_MASK) | flags;
  }
}

void map_range_identity(PageAllocator *alloc, PageTable *level4_table,
    size_t first_page_addr, size_t size_in_bytes, size_t flags) {
  ASSERT(!(first_page_addr % PAGE_SIZE));
  for (size_t addr = first_page_addr;
      addr < first_page_addr + size_in_bytes;
      addr += PAGE_SIZE) {
    map_page_identity(alloc, level4_table, addr, flags);
  }
}

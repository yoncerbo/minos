#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

PageAllocator2 *GLOBAL_PAGE_ALLOCATOR = 0;

// Returns zeroed page
paddr_t alloc_pages(PageAllocator *allocator, size_t page_count) {
  ASSERT(allocator->page_offset + page_count < allocator->page_count);
  paddr_t addr = allocator->start + allocator->page_offset * PAGE_SIZE;
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

void map_page(PageAllocator2 *alloc, PageTable *pml4, paddr_t physical, vaddr_t virtual, size_t flags) {
  ASSERT(physical % PAGE_SIZE == 0);
  ASSERT(virtual % PAGE_SIZE == 0);

  uint32_t page_table_indices[4] = {
    (virtual >> 39) % 512,
    (virtual >> 30) % 512,
    (virtual >> 21) % 512,
    (virtual >> 12) % 512,
  };

  flags |= PAGE_BIT_PRESENT;

  PageTable *page_table = pml4;
  for (int i = 0; i < 3; ++i) {
    uint32_t index = page_table_indices[i];
    size_t entry = page_table->entries[index];
    if (!(entry & PAGE_BIT_PRESENT)) {
      paddr_t table_addr = alloc_pages2(alloc, 1);
      memset((void *)table_addr, 0, PAGE_SIZE);
      page_table->entries[index] = (table_addr & PAGE_ADDR_MASK) | flags;
    }
    page_table = (void *)(page_table->entries[index] & PAGE_ADDR_MASK);
  }

  size_t entry = page_table->entries[page_table_indices[3]];
  if (!(entry & PAGE_BIT_PRESENT)) {
    page_table->entries[page_table_indices[3]] = (physical & PAGE_ADDR_MASK) | flags;
  }
}

void map_pages(PageAllocator2 *alloc, PageTable *pml4, paddr_t physical_start, vaddr_t virtual_start,
    size_t size_in_bytes, size_t flags) {
  paddr_t end = physical_start + size_in_bytes;
  for (; physical_start < end; physical_start += PAGE_SIZE, virtual_start += PAGE_SIZE) {
    map_page(alloc, pml4, physical_start, virtual_start, flags);
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

void push_free_pages(PageAllocator2 *alloc, paddr_t physical_start, size_t page_count) {
  ASSERT(physical_start % PAGE_SIZE == 0);
  ASSERT(alloc->capacity > alloc->len);

  size_t new_range_end = physical_start + page_count * PAGE_SIZE;

  for (uint32_t i = 0; i < alloc->len; ++i) {
    PhysicalPageRange *range = &alloc->ranges[i];
    if (new_range_end == range->start) {
      range->start = physical_start;
      range->page_count += page_count;
      return;
    }
    if (physical_start == range->start + range->page_count * PAGE_SIZE) {
      range->page_count += page_count;
      return;
    }
  }

  alloc->ranges[alloc->len++] = (PhysicalPageRange){physical_start, page_count};
}

paddr_t alloc_pages2(PageAllocator2 *alloc, size_t page_count) {
  for (uint32_t i = 0; i < alloc->len; ++i) {
    PhysicalPageRange *range = &alloc->ranges[i];
    if (range->page_count == page_count) {
      paddr_t addr = range->start;
      *range = alloc->ranges[--alloc->len];
      return addr;
    } else if (range->page_count > page_count) {
      paddr_t addr = range->start;
      range->page_count -= page_count;
      range->start += page_count * PAGE_SIZE;
      return addr;
    }
  }
  log("Failed to allocate %d physical pages", page_count);
  ASSERT(0);
}

uint32_t push_virtual_object(MemoryManager *mm, VirtualObject obj) {
  ASSERT(mm->objects_count < MAX_VIRTUAL_OBJECTS);
  uint32_t index = mm->objects_count++;
  mm->objects[index] = obj;
  return index;
}

void map_virtual_range(MemoryManager *mm, vaddr_t virtual, paddr_t physical, size_t size, size_t flags) {
  ASSERT(virtual >= mm->start);
  ASSERT(virtual + size <= mm->end);

  uint32_t *obj_index = &mm->first_object;

  while (*obj_index) {
    VirtualObject *obj = &mm->objects[*obj_index];
    vaddr_t new_end = virtual + size;
    if (new_end <= obj->virtual) break;
    ASSERT(virtual >= obj->virtual + obj->size);
    obj_index = &obj->next;
  }

  VirtualObject obj = {virtual, physical, size, *obj_index};
  uint32_t index = push_virtual_object(mm, obj);
  *obj_index = index;
  map_pages(mm->page_alloc, mm->pml4, physical, virtual, size, flags);
}

void flush_page_table(MemoryManager *mm) {
  ASM("mov cr3, %0" :: "r"((size_t)mm->pml4 & PAGE_ADDR_MASK));
}

void alloc_virtual(MemoryManager *mm, vaddr_t virtual, size_t size, size_t flags) {
  if (!size) return;
  paddr_t physical = alloc_pages2(mm->page_alloc, (size + PAGE_SIZE - 1) / PAGE_SIZE);
  map_virtual_range(mm, virtual, physical, size, flags);
}

#ifndef INCLUDE_MEMORY
#define INCLUDE_MEMORY

#include "common.h"

typedef size_t paddr_t;
typedef size_t vaddr_t;

#define SATP_SV32 (1u << 31)

typedef enum {
  PAGE_V = (1 << 0),
  PAGE_R = (1 << 1),
  PAGE_W = (1 << 2),
  PAGE_X = (1 << 3),
  PAGE_U = (1 << 4),
  PAGE_G = (1 << 5),
  PAGE_A = (1 << 6),
  PAGE_D = (1 << 7),

  // 2 bits for os use
  // 22 bits page number
} PageEntryFlags;

paddr_t alloc_pages(uint32_t count);
void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags);

#endif


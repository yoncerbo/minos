#ifndef INCLUDE_MEMORY
#define INCLUDE_MEMORY

#include "common.h"

typedef size_t paddr_t;
typedef size_t vaddr_t;

#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0)
#define PAGE_R (1 << 1)
#define PAGE_W (1 << 2)
#define PAGE_X (1 << 3)
#define PAGE_U (1 << 4)

paddr_t alloc_pages(uint32_t count);
void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags);

#endif


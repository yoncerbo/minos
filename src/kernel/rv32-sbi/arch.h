#ifndef INCLUDE_RV32_ARCH
#define INCLUDE_RV32_ARCH

#include "common.h"

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

#define WFI() __asm__ __volatile__ ("wfi")
#define PAGE_SIZE 4096

// src/uart.c
void uart_init(void);

// src/sbi.c
// NOTE: assembly source in boot.s
// It would be more efficient to use inline assembly,
// but those functions won't be called a lot, so it doesn't
// matter. Just a note for the future.

extern long sbi_set_timer(uint64_t stime_value);
extern long sbi_console_putchar(char ch);
extern long sbi_console_getchar(void);
extern long sbi_shutdown(void);

// src/plic.c
void plic_enable(uint32_t id);
void plic_set_priority(uint32_t id, uint8_t priority);
void plic_set_threshold(uint8_t threshold);
uint32_t plic_claim(void);
void plic_complete(uint32_t id);
void plic_enablep(uint32_t id, uint8_t priority);

// src/memory.c
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

volatile uint8_t * const UART = (void *)0x10000000;
const uint32_t UART_INT = 10;

const uint32_t VIRTIO_MMIO_START = 0x10001000;
const uint32_t VIRTIO_INTERRUPT_START = 0;
const uint32_t VIRTIO_COUNT = 7;

#endif

#ifndef INCLUDE_X64_ARCH
#define INCLUDE_X64_ARCH

#include "common.h"

#define PAGE_SIZE 4096
#define WFI() __asm__ __volatile__("hlt")

#define SYSV __attribute__((sysv_abi))

typedef struct PACKED {
  uint32_t reserved0;
  uint64_t rsp0, rsp1, rsp2;
  uint64_t reserved1;
  uint64_t ist1, inst2, ist3, ist4, ist5, ist6, ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iopb_offset;
} Tss;

Tss TSS;

typedef struct PACKED {
  uint16_t limit0_15;
  uint16_t base0_15;
  uint8_t base16_23;
  uint8_t access_byte;
  uint8_t limit16_19_flags;
  uint8_t base24_31;
} GdtEntry;

#define GDT_ENTRY(base, limit, access_byte, flags) \
  { (uint16_t)(limit), (uint16_t)(base), (base) >> 16, (access_byte), (((limit) >> 16) & 0x0F) | ((flags) << 4), (base) >> 24 }

// SOURCE: https://wiki.osdev.org/GDT_Tutorial
// SOURCE: https://wiki.osdev.org/Global_Descriptor_Table
// NOTE: In long mode base and limit are ignored, each descriptor covers the whole memory
ALIGNED(PAGE_SIZE) GdtEntry GDT_TABLE[7] = {
  {0}, // null entry
  GDT_ENTRY(0, 0, 0x9A, 0xA), // kernel mode code segment
  GDT_ENTRY(0, 0, 0x92, 0xA), // kernel mode data segment
  GDT_ENTRY(0, 0, 0xFA, 0xA), // user mode code segment
  GDT_ENTRY(0, 0, 0xF2, 0xA), // user mode data segment
  {0}, // TSS segment low
  {0}, // TSS segment high
};

typedef struct PACKED {
  uint16_t limit;
  uint64_t base;
} GdtTablePtr;

SYSV extern void load_gdt_table(GdtTablePtr *gdt_table_ptr);

#endif

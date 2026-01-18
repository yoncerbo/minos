#ifndef INCLUDE_X64_ARCH
#define INCLUDE_X64_ARCH

#include "common.h"
#include "efi.h"

#define PAGE_SIZE 4096
#define WFI() __asm__ __volatile__("hlt")

typedef struct PACKED {
  uint32_t reserved0;
  uint64_t rsp0, rsp1, rsp2;
  uint64_t reserved1;
  uint64_t ist1, inst2, ist3, ist4, ist5, ist6, ist7;
  uint64_t reserved2;
  uint16_t reserved3;
  uint16_t iopb_offset;
} Tss;

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

typedef enum {
  GDT_KERNEL_NULL = 0,
  GDT_KERNEL_CODE = 1,
  GDT_KERNEL_DATA = 2,
  GDT_USER_CODE = 3,
  GDT_USER_DATA = 4,
  GDT_TSS_LOW = 5,
  GDT_TSS_HIGH = 6,
  GDT_COUNT,
} GdtEntryIndex;

// SOURCE: https://wiki.osdev.org/GDT_Tutorial
// SOURCE: https://wiki.osdev.org/Global_Descriptor_Table
// NOTE: In long mode base and limit are ignored, each descriptor covers the whole memory
ALIGNED(PAGE_SIZE) GdtEntry GDT_TABLE[GDT_COUNT] = {
  [GDT_KERNEL_CODE] = GDT_ENTRY(0, 0, 0x9A, 0xA),
  [GDT_KERNEL_DATA] = GDT_ENTRY(0, 0, 0x92, 0xA),
  [GDT_USER_CODE] = GDT_ENTRY(0, 0, 0xFA, 0xA),
  [GDT_USER_DATA] = GDT_ENTRY(0, 0, 0xF2, 0xA),
};

typedef struct PACKED {
  uint16_t limit;
  uint64_t base;
} GdtPtr;

// SOURCE: https://wiki.osdev.org/Interrupt_Descriptor_Table#Structure_on_x86-64
// SOURCE: https://wiki.osdev.org/Interrupts_Tutorial
typedef struct PACKED {
  uint16_t isr0_15;
  uint16_t gdt_selector;
  uint8_t interrupt_stack_table; // only first 2 bits, the rest are 0
  uint8_t type_attributes;
  uint16_t isr16_31;
  uint32_t isr32_63;
  uint32_t reserved;
} InterruptDescriptor;

typedef enum {
  IDT_DIVISION_ERROR = 0,
  IDT_BREAKPOINT = 3,
  IDT_INVALID_OPCODE = 6,
  IDT_DOUBLE_FAULT = 8,
  IDT_INVALID_TSS = 10,
  IDT_GENERAL_PROTECTION = 13,
  IDT_PAGE_FAULT = 14,
  IDT_IRQ_START = 32,
} IdtIndex;

typedef struct PACKED {
  uint16_t limit;
  InterruptDescriptor *base;
} IdtPtr;

typedef struct {
  size_t ip, cs, flags, sp, ss;
} InterruptFrame;

#define INTERRUPT __attribute__((interrupt))

typedef struct PACKED {
  uint64_t entries[512];
} PageTable;

// SOURCE: https://wiki.osdev.org/Paging#/media/File:64-bit_page_tables2.png
#define PAGE_BIT_PRESENT ((size_t)1 << 0)
#define PAGE_BIT_WRITABLE ((size_t)1 << 1)
#define PAGE_BIT_USER ((size_t)1 << 2)
#define PAGE_BIT_NOT_EXECUTABLE ((size_t)1 << 63)

#define PAGE_ADDR_MASK 0x000ffffffffff000ull

size_t efi_setup(void *image_handle, EfiSystemTable *st, Surface *surface, uint8_t *memory_map, size_t *memory_map_size, size_t *memory_descriptor_size);

void setup_idt(InterruptDescriptor *idt);

typedef struct {
  paddr_t start;
  size_t page_count;
  size_t page_offset;
} PageAllocator;

PageAllocator GLOBAL_PAGE_ALLOCATOR = {0};

paddr_t alloc_pages(PageAllocator *allocator, size_t page_count);
void map_page_identity(PageAllocator *alloc, PageTable *level4_table, size_t addr, size_t flags);
void map_range_identity(PageAllocator *alloc, PageTable *level4_table,
    size_t first_page_addr, size_t size_in_bytes, size_t flags);

SYSV extern void load_gdt_table(GdtPtr *gdt_table_ptr);
SYSV extern void enable_system_calls(void *kernel_gs_base);
SYSV extern size_t run_user_program(void (*user_main)(void), void *stack_top);
SYSV extern void load_page_table(PageTable *level4_table);
SYSV extern void putchar_qemu_debugcon(char ch);

ALIGNED(PAGE_SIZE) PageTable PML4;

typedef enum {
  SYS_LOG = 1,
  SYS_EXIT = 2,
} SyscallType;

typedef enum {
  SYS_OK = 0,
  SYS_ERR_UNKNOWN_SYSCALL = 1,
  SYS_ERR_BAD_ARG = 2,
} SyscallError;

SYSV SyscallError sys_log(const char *str, size_t limit);
NORETURN SYSV void sys_exit(size_t error_code);

typedef struct {
  size_t kernel_sp;
  size_t user_sp;
} KernelThreadContext;

NORETURN void user_main(void);

#endif

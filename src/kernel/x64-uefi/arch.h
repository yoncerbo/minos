#ifndef INCLUDE_X64_ARCH
#define INCLUDE_X64_ARCH

#include "cmn/lib.h"
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
  GDT_KERNEL_CODE = 1, // 0x08
  GDT_KERNEL_DATA = 2, // 0x10
  // NOTE: "SYSRET sets the CS selector value to MSR IA32_STAR[63:48] +16. The SS is set to IA32_STAR[63:48] + 8."
  GDT_USER_NULL = 3, // 0x18
  GDT_USER_DATA = 4, // +8 - for ss
  GDT_USER_CODE = 5, // +16 - for cs
  GDT_TSS_LOW = 6,
  GDT_TSS_HIGH = 7,
  GDT_COUNT,
} GdtEntryIndex;

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
  PageTable *pml4;
} PageAllocator;

paddr_t alloc_pages(PageAllocator *allocator, size_t page_count);
void map_page_identity(PageAllocator *alloc, PageTable *level4_table, size_t addr, size_t flags);
void map_range_identity(PageAllocator *alloc, PageTable *level4_table,
    size_t first_page_addr, size_t size_in_bytes, size_t flags);

SYSV extern void load_gdt_table(GdtPtr *gdt_table_ptr);
SYSV extern void enable_system_calls(void *kernel_gs_base);
SYSV extern size_t run_user_program(void *function, void *stack_top);
SYSV extern void putchar_qemu_debugcon(char ch);

ALIGNED(PAGE_SIZE) PageTable PML4;

typedef struct {
  size_t kernel_sp;
  size_t user_sp;
} KernelThreadContext;

NORETURN void user_main(void);

struct {
  Surface fb;
  size_t memory_map_size;
  size_t memory_descriptor_size;
  size_t image_base;
} BOOT_CONFIG;

typedef struct PACKED {
  char signature[8];
  uint8_t checksum;
  char oemid[6];
  uint8_t revision;
  uint32_t rsdt_address;
} Rsdp;

typedef struct PACKED {
  char signature[4];
  uint32_t length;
  uint8_t revision;
  uint8_t checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} SdtHeader;

#define WRITE_PORT(port, value) ASM("out %0, %1" :: "i"(port), "a"(value));
#define READ_PORT(port, value) ASM("in %0, %1" : "=a"(value) : "i"(port));

#define WRITE_MSR(msr, low32, high32) ASM("wrmsr" :: "c"(msr), "a"(low32), "d"(high32))
#define READ_MSR(msr, low32, high32) ASM("rdmsr" : "=a"(low32), "=d"(high32) : "c"(msr))

typedef struct {
  Sink sink;
  Font font;
  Surface surface;
  uint32_t x, y;
  uint32_t line_spacing;
} FbSink;

FbSink FB_SINK;
Sink QEMU_DEBUGCON_SINK;

#define GS_RELATIVE __attribute__((address_space(256)))
GS_RELATIVE const KernelThreadContext *CONTEXT = 0;

typedef struct {
  size_t start;
  size_t page_count;
} PhysicalPageRange;

typedef struct {
  PhysicalPageRange *ranges;
  uint32_t capacity;
  uint32_t len;
} PageAllocator2;

void push_free_pages(PageAllocator2 *alloc, size_t physical_start, size_t page_count);
paddr_t alloc_pages2(PageAllocator2 *alloc, size_t page_count);

ALIGNED(16) InterruptDescriptor IDT[256] = {0};
Tss TSS;

void setup_gdt_and_tss(GdtEntry gdt[GDT_COUNT], Tss *tss, void *interrupt_stack_top);

typedef struct {
  GdtEntry gdt[GDT_COUNT];
  Tss tss;
  ALIGNED(16) InterruptDescriptor idt[256];
  PageTable *pml4;
  PageAllocator2 alloc;
  Surface fb;
  Font font;
  uint8_t *user_efi_file;
  size_t io_apic_addr;
  uint32_t pit_interrupt;
} BootData;

void validate_elf_header(ElfHeader64 *elf);
void load_elf_file(PageAllocator2 *alloc, PageTable *pml4, void *file, vaddr_t *out_entry);

enum {
  APIC_LOCAL_ID = 0x20 / 4,
  APIC_END_OF_INTERRUPT = 0xB0 / 4,
  APIC_SPURIOUS_VECTOR = 0xF0 / 4,
  APIC_LVT = 0x320 / 4,
  APIC_TIMER_TICKS = 0x380 / 4,
};

enum {
  IO_APIC_ID = 0,
  IO_APIC_VER = 1,
  IO_APIC_ARB = 2,
  IO_APIC_REDTBL = 3,
};

uint32_t setup_apic(PageAllocator2 *alloc, PageTable *pml4);
volatile uint32_t *get_apic_regs(void);
uint32_t read_ioapic_register(uint32_t io_apic_addr, uint32_t register_select);
void write_ioapic_register(uint32_t io_apic_addr, uint32_t register_select, uint32_t value);

#endif

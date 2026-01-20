#include "cmn/lib.h"
#include "common.h"
#include "arch.h"
#include "efi.h"

// Source files
#include "font.h"
#include "interrupts.c"
#include "drawing.c"
#include "efi.c"
#include "memory.c"
#include "syscalls.c"
#include "user.c"
#include "logging.c"
#include "print.c"

// TODO: Better logging system, thread-safe for interrupts, multiple targets
// TODO: Loading user program from disk
// TODO: Running on real hardware
// TODO: Arena allocator for the kernel

#define MAX_PHYSICAL_RANGES 256
PhysicalPageRange PAGE_RANGES[MAX_PHYSICAL_RANGES];

ALIGNED(16) InterruptDescriptor IDT[256] = {0};
Tss TSS;

// TODO: Only the binary and stack should be mapped with user bit
extern char USER_BINARY[], USER_BINARY_END[];

// TODO: Setup additional pages for protecting the stack
ALIGNED(16) uint8_t USER_STACK[8 * 1024];
ALIGNED(16) uint8_t INTERRUPT_STACK[8 * 1024];

NORETURN void kernel_main(void) {
  DEBUGX(BOOT_CONFIG.image_base);

  int count = BOOT_CONFIG.memory_map_size / BOOT_CONFIG.memory_descriptor_size;

  // SOURCE: https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#memory-type-usage-after-exitbootservices
 
  PageAllocator allocator = {0};
  EfiMemoryDescriptor *kernel_code = 0;

  PageAllocator2 alloc2 = {
    .capacity = MAX_PHYSICAL_RANGES,
    .ranges = PAGE_RANGES,
  };
  GLOBAL_PAGE_ALLOCATOR = &alloc2;

  for (size_t offset = 0;
      offset < BOOT_CONFIG.memory_map_size;
      offset += BOOT_CONFIG.memory_descriptor_size) {
    EfiMemoryDescriptor *desc = (void *)(MEMORY_MAP + offset);
    // NOTE: We only handle one code section for now
    ASSERT(desc->type != EFI_LOADER_DATA);
    if (desc->type == EFI_LOADER_CODE) {
      kernel_code = desc;
    } else if (desc->type == EFI_CONVENTIONAL_MEMORY) {
      if (desc->number_of_pages <= allocator.page_count) continue;
      allocator.page_count = desc->number_of_pages;
      allocator.start = desc->physical_start;
    }
    if (desc->type == EFI_CONVENTIONAL_MEMORY ) {
        // NOTE: We can't write to those areas before we set up our page tables
        // desc->type == EFI_BOOT_SERVICES_CODE || desc->type == EFI_BOOT_SERVICES_DATA) {
      push_free_pages(&alloc2, desc->physical_start, desc->number_of_pages);
    }
  }

  paddr_t biggest_memory_addr = 0;
  size_t pages_total = 0;
  for (size_t i = 0; i < alloc2.len; ++i) {
    log("Memory %d addr=%X, pages=%d", i, alloc2.ranges[i].start, alloc2.ranges[i].page_count);
    pages_total += alloc2.ranges[i].page_count;
    paddr_t end = alloc2.ranges[i].start + alloc2.ranges[i].page_count * PAGE_SIZE;
    if (end > biggest_memory_addr) biggest_memory_addr = end;
  }
  DEBUGD(pages_total);

  ASSERT(allocator.page_count);
  ASSERT(allocator.start % PAGE_SIZE == 0);

  log("Memory info: count=%d, addr=0x%x", allocator.page_count, allocator.start);

  ASM("cli");
  log("Interrupts disabled");

  TSS.ist1 = (size_t)&INTERRUPT_STACK[sizeof(INTERRUPT_STACK) - 1];
  uint64_t tss_base = (uint64_t)&TSS;
  GDT_TABLE[GDT_TSS_LOW] = (GdtEntry)GDT_ENTRY(tss_base, sizeof(TSS) - 1, 0x89, 0xA);
  // *(uint32_t *)&GDT_TABLE[7] = (uint32_t)(tss_base >> 32);
  GDT_TABLE[GDT_TSS_HIGH].limit0_15 = (tss_base >> 32) & 0xFFFF;
  GDT_TABLE[GDT_TSS_HIGH].base0_15 = (tss_base >> 48) & 0xFFFF;

  GdtPtr gdt_table_ptr = { sizeof(GDT_TABLE) - 1, (uint64_t)&GDT_TABLE };
  load_gdt_table(&gdt_table_ptr);

  log("Setup GDT and TSS");

  setup_idt(IDT);
  log("Setup IDT");

  PageTable *pml4 = (void *)alloc_pages(&allocator, 1);
  memset(pml4, 0, PAGE_SIZE);
  size_t memory_size = 1024 * 1024 * 1024;
  size_t page_flags = PAGE_BIT_WRITABLE | PAGE_BIT_USER | PAGE_BIT_PRESENT;

  // map_range_identity(&allocator, pml4, kernel_code->physical_start, kernel_code->number_of_pages * PAGE_SIZE, page_flags);
  // map_range_identity(&allocator, pml4, (size_t)BOOT_CONFIG.fb.ptr,
  //     BOOT_CONFIG.fb.height * BOOT_CONFIG.fb.pitch, page_flags);

  map_pages(&alloc2, pml4, kernel_code->physical_start, kernel_code->physical_start,
      kernel_code->number_of_pages * PAGE_SIZE, page_flags);

  size_t virtual_offset = 0xFFFF800000000000;
  map_pages(&alloc2, pml4, 0, virtual_offset, PAGE_SIZE, page_flags);

  map_pages(&alloc2, pml4, (size_t)BOOT_CONFIG.fb.ptr, (size_t)BOOT_CONFIG.fb.ptr,
      BOOT_CONFIG.fb.height * BOOT_CONFIG.fb.pitch, PAGE_BIT_WRITABLE | PAGE_BIT_PRESENT);

  ASM( "mov cr3, %0" :: "r"((size_t)pml4 & PAGE_ADDR_MASK));

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  log("Running user program");
  size_t status;
  status = run_user_program((void *)USER_BINARY, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);

  log("OK");

  for (;;) WFI();
}

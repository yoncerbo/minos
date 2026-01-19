#include "common.h"
#include "arch.h"
#include "efi.h"

// Source files
#include "interrupts.c"
#include "strings.c"
#include "drawing.c"
#include "efi.c"
#include "memory.c"
#include "syscalls.c"
#include "user.c"
#include "print.c"
#include "logging.c"

// TODO: Better logging system, thread-safe for interrupts, multiple targets
// TODO: Loading user program from disk
// TODO: Running on real hardware

ALIGNED(16) InterruptDescriptor IDT[256] = {0};
Tss TSS;

// TODO: Setup additional pages for protecting the stack
ALIGNED(16) uint8_t USER_STACK[8 * 1024];
ALIGNED(16) uint8_t INTERRUPT_STACK[8 * 1024];

NORETURN void kernel_main(void) {
  DEBUGX(BOOT_CONFIG.image_base);

  int count = BOOT_CONFIG.memory_map_size / BOOT_CONFIG.memory_descriptor_size;

  // SOURCE: https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#memory-type-usage-after-exitbootservices
 
  PageAllocator allocator = {0};
  EfiMemoryDescriptor *kernel_code = 0;

  for (size_t offset = 0;
      offset < BOOT_CONFIG.memory_map_size;
      offset += BOOT_CONFIG.memory_descriptor_size) {
    EfiMemoryDescriptor *desc = (void *)(MEMORY_MAP + offset);
    // NOTE: We only handle one code section for now
    ASSERT(desc->type != EFI_LOADER_DATA);
    if (desc->type == EFI_LOADER_DATA) {
      log("Kernel data: addr=0x%X, count=%d", desc->physical_start, desc->number_of_pages);
    } else if (desc->type == EFI_LOADER_CODE) {
      log("Kernel code: addr=0x%X, count=%d", desc->physical_start, desc->number_of_pages);
      ASSERT(kernel_code == NULL);
      kernel_code = desc;
    } else if (desc->type == EFI_CONVENTIONAL_MEMORY) {
      log("Memory: addr=0x%X, count=%d", desc->physical_start, desc->number_of_pages);
      if (desc->number_of_pages <= allocator.page_count) continue;
      allocator.page_count = desc->number_of_pages;
      allocator.start = desc->physical_start;
    }
  }

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
  size_t page_flags = PAGE_BIT_WRITABLE | PAGE_BIT_USER;

  map_range_identity(&allocator, pml4, kernel_code->physical_start, kernel_code->number_of_pages * PAGE_SIZE, page_flags);
  map_range_identity(&allocator, pml4, (size_t)BOOT_CONFIG.fb.ptr,
      BOOT_CONFIG.fb.height * BOOT_CONFIG.fb.pitch, page_flags);

  ASM( "mov cr3, %0" :: "r"((size_t)pml4 & PAGE_ADDR_MASK));

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  log("Running user program");
  size_t status;
  status = run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);
  status = run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);

  log("Back in kernel");

  for (;;) WFI();
}


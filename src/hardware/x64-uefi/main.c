#include "common.h"
#include "arch.h"
#include "efi.h"

// Source files
#include "interrupts.c"
#include "strings.c"
#include "drawing.c"
#include "print.c"
#include "efi.c"
#include "memory.c"
#include "syscalls.c"
#include "user.c"

// TODO: Better logging system, thread-safe for interrupts, multiple targets
// TODO: Loading user program from disk
// TODO: Running on real hardware

#define ASM_OUT(port, value) ASM("out " #port ", %0" : : "a" (value));

EfiSimpleTextOutputProtocol *efiout;

SYSV void putchar_efi(char ch) {
  if (ch == '\n') {
    efiout->output_string(efiout, L"\n\r");
  } else {
    efiout->output_string(efiout, (wchar_t[]){ch, 0});
  }
}

Surface BOOT_SURFACE = {0};
SYSV void putchar_surface(char ch) {
  const uint32_t MARGIN = 16;
  const uint32_t HEIGHT = 16;
  const uint32_t WIDTH = 8;

  static uint32_t x = MARGIN; 
  static uint32_t y = MARGIN;

  if (x >= BOOT_SURFACE.width) {
    x = MARGIN;
    y += HEIGHT;
  }

  if (y >= BOOT_SURFACE.height) {
    return;
  }

  if (ch == '\n') {
    x = MARGIN;
    y += HEIGHT;
  } else {
    draw_char(&BOOT_SURFACE, x, y, WHITE, ch);
    x += WIDTH;
  }
}

uint8_t MEMORY_MAP[1024 * 32];
ALIGNED(16) InterruptDescriptor IDT[256] = {0};
Tss TSS;

// TODO: Setup additional pages for protecting the stack
ALIGNED(16) uint8_t USER_STACK[8 * 1024];
ALIGNED(16) uint8_t INTERRUPT_STACK[8 * 1024];
ALIGNED(16) uint8_t KERNEL_STACK[8 * 1024];

typedef struct {
  PageTable *pml4;
  PageAllocator allocator;
} KernelConfig;
CASSERT(sizeof(KernelConfig) < PAGE_SIZE);

NORETURN SYSV void kernel_main(KernelConfig *config) {
  printf("Hello from kernel!\n");
  LOG("Setup paging!\n", 0);

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  size_t status;
  status = run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);
  status = run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  DEBUGD(status);

  printf("Back in kernel\n");

  for (;;) WFI();
}

size_t efi_main(void *image_handle, EfiSystemTable *st) {
  efiout = st->con_out;
  putchar = putchar_efi;

  size_t memory_map_size = sizeof(MEMORY_MAP);
  size_t memory_descriptor_size;

  size_t status = efi_setup(image_handle, st,
      &BOOT_SURFACE, MEMORY_MAP, &memory_map_size, &memory_descriptor_size);

  if (IS_EFI_ERROR(status)) {
    printf("ERROR: Failed efi setup\n");
    for (;;) WFI();
  }

  putchar = putchar_surface;

  int count = memory_map_size / memory_descriptor_size;

  // SOURCE: https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#memory-type-usage-after-exitbootservices
 
  PageAllocator allocator = {0};
  EfiMemoryDescriptor *kernel_code = 0;

  for (size_t offset = 0; offset < memory_map_size; offset += memory_descriptor_size) {
    EfiMemoryDescriptor *desc = (void *)(MEMORY_MAP + offset);
    // NOTE: We only handle one code section for now
    ASSERT(desc->type != EFI_LOADER_DATA);
    if (desc->type == EFI_LOADER_DATA) {
      printf("Kernel data: addr=0x%X, count=%d\n", desc->physical_start, desc->number_of_pages);
    } else if (desc->type == EFI_LOADER_CODE) {
      printf("Kernel code: addr=0x%X, count=%d\n", desc->physical_start, desc->number_of_pages);
      ASSERT(kernel_code == NULL);
      kernel_code = desc;
    } else if (desc->type == EFI_CONVENTIONAL_MEMORY) {
      printf("Memory: addr=0x%X, count=%d\n", desc->physical_start, desc->number_of_pages);
      if (desc->number_of_pages <= allocator.page_count) continue;
      allocator.page_count = desc->number_of_pages;
      allocator.start = desc->physical_start;
    }
  }

  ASSERT(allocator.page_count);
  ASSERT(allocator.start % PAGE_SIZE == 0);

  printf("Memory info: count=%d, addr=0x%x\n", allocator.page_count, allocator.start);

  ASM("cli");
  LOG("Interrupts disabled\n", 0);

  TSS.ist1 = (size_t)&INTERRUPT_STACK[sizeof(INTERRUPT_STACK) - 1];
  uint64_t tss_base = (uint64_t)&TSS;
  GDT_TABLE[GDT_TSS_LOW] = (GdtEntry)GDT_ENTRY(tss_base, sizeof(TSS) - 1, 0x89, 0xA);
  // *(uint32_t *)&GDT_TABLE[7] = (uint32_t)(tss_base >> 32);
  GDT_TABLE[GDT_TSS_HIGH].limit0_15 = (tss_base >> 32) & 0xFFFF;
  GDT_TABLE[GDT_TSS_HIGH].base0_15 = (tss_base >> 48) & 0xFFFF;

  GdtPtr gdt_table_ptr = { sizeof(GDT_TABLE) - 1, (uint64_t)&GDT_TABLE };
  load_gdt_table(&gdt_table_ptr);

  LOG("Setup GDT and TSS\n", 0);

  setup_idt(IDT);
  LOG("Setup IDT\n", 0);

  PageTable *pml4 = (void *)alloc_pages(&allocator, 1);
  memset(pml4, 0, PAGE_SIZE);
  size_t memory_size = 1024 * 1024 * 1024;
  size_t page_flags = PAGE_BIT_WRITABLE | PAGE_BIT_USER;

  map_range_identity(&allocator, pml4, kernel_code->physical_start, kernel_code->number_of_pages * PAGE_SIZE, page_flags);
  map_range_identity(&allocator, pml4, (size_t)BOOT_SURFACE.ptr,
      BOOT_SURFACE.height * BOOT_SURFACE.pitch, page_flags);

  KernelConfig *config = (void *)alloc_pages(&allocator, 1);
  config->pml4 = pml4;
  config->allocator = allocator;

  // NOTE: After switching to the new page table, we can't use the current stack pointer,
  // because it points the unmapped memory region - UEFI data
  // we have to set up a new stack
  ASM("mov cr3, %0" :: "r"((size_t)pml4 & PAGE_ADDR_MASK));
  ASM("mov rsp, %0" :: "g"(&KERNEL_STACK[sizeof(KERNEL_STACK) - 1]));
  ASM("jmp kernel_main" :: "D"(config));
  UNREACHABLE();
}


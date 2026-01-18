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

#define ASM_OUT(port, value) ASM("out " #port ", %0" : : "a" (value));

Surface *log_surface = {0};

SYSV void putchar_surface(char ch) {
  const uint32_t MARGIN = 16;
  const uint32_t HEIGHT = 16;
  const uint32_t WIDTH = 8;

  static uint32_t x = MARGIN; 
  static uint32_t y = MARGIN;

  if (x >= log_surface->width) {
    x = MARGIN;
    y += HEIGHT;
  }

  if (y >= log_surface->height) {
    return;
  }

  if (ch == '\n') {
    x = MARGIN;
    y += HEIGHT;
  } else {
    draw_char(log_surface, x, y, WHITE, ch);
    x += WIDTH;
  }
}

uint8_t USER_STACK[8 * 1024];
uint8_t MEMORY_MAP[1024 * 32];
ALIGNED(16) InterruptDescriptor IDT[256] = {0};
Tss TSS;

uint8_t interrupt_stack[8 * 1024];

size_t efi_main(void *image_handle, EfiSystemTable *st) {
  putchar = putchar_qemu_debugcon;

  size_t memory_map_size = sizeof(MEMORY_MAP);
  Surface surface;
  size_t memory_descriptor_size;

  size_t status = efi_setup(image_handle, st, &surface, MEMORY_MAP, &memory_map_size, &memory_descriptor_size);
  if (IS_EFI_ERROR(status)) return status;

  log_surface = &surface;
  putchar = putchar_surface;

  PageAllocator allocator = {0};

  int count = memory_map_size / memory_descriptor_size;

  for (size_t offset = 0; offset < memory_map_size; offset += memory_descriptor_size) {
    EfiMemoryDescriptor *desc = (void *)(MEMORY_MAP + offset);
    if (desc->type == EFI_LOADER_DATA) {
      printf("Kernel data: count=%d, p=0x%X, v=0x%X\n",
           desc->number_of_pages, desc->physical_start, desc->virtual_start);
    } else if (desc->type == EFI_LOADER_CODE) {
      printf("Kernel code: count=%d, p=0x%X, v=0x%X\n",
           desc->number_of_pages, desc->physical_start, desc->virtual_start);
    } else if (desc->type == EFI_CONVENTIONAL_MEMORY) {
      printf("Conventional: count=%d, p=0x%X, v=0x%X\n",
           desc->number_of_pages, desc->physical_start, desc->virtual_start);
    }
    if (desc->type != EFI_CONVENTIONAL_MEMORY) continue;
    if (desc->number_of_pages <= allocator.page_count) continue;
    allocator.page_count = desc->number_of_pages;
    allocator.start = desc->physical_start;
  }

  ASSERT(allocator.page_count);
  ASSERT(allocator.start % PAGE_SIZE == 0);
  GLOBAL_PAGE_ALLOCATOR = allocator;

  printf("Memory info: count=%d, addr=0x%x\n", allocator.page_count, allocator.start);

  ASM("cli");
  LOG("Interrupts disable\n", 0);

  TSS.ist1 = (size_t)&interrupt_stack[sizeof(interrupt_stack) - 1];
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
  size_t memory_size = 1024 * 1024 * 1024;
  size_t page_flags = PAGE_BIT_WRITABLE | PAGE_BIT_USER;
  map_range_identity(&allocator, pml4, 0, memory_size, page_flags);
  map_range_identity(&allocator, pml4, (size_t)surface.ptr, surface.height * surface.pitch, page_flags);
  load_page_table(pml4);
  
  LOG("Setup paging!\n", 0);

  KernelThreadContext thread_context = {0};
  enable_system_calls(&thread_context);

  run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);

  printf("Back in kernel\n");

  run_user_program(user_main, (void *)&USER_STACK[sizeof(USER_STACK) - 1]);
  
  for (;;) {
    WFI();
  }
}


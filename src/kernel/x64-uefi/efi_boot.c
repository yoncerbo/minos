#include "cmn/lib.h"
#include "common.h"
#include "efi.h"
#include "arch.h"

// Source files
#include "interrupts.c"
#include "drawing.c"
#include "memory.c"
#include "syscalls.c"
#include "user.c"
#include "logging.c"
#include "elf.c"
#include "gdt.c"

#define MAX_PHYSICAL_RANGES 256
PhysicalPageRange PAGE_RANGES[MAX_PHYSICAL_RANGES];

CASSERT(sizeof(EfiGuid) == sizeof(uint64_t[2]));
bool are_efi_guid_equal(const EfiGuid *a, const EfiGuid *b) {
  uint64_t *arr_a = (void *)a;
  uint64_t *arr_b = (void *)b;
  return arr_a[0] == arr_b[0] && arr_a[1] == arr_b[1];
}

EfiFileHandle *open_efi_file(EfiFileHandle *parent, const wchar_t *filename, uint64_t file_mode, uint64_t attributes) {
  EfiFileHandle *file;
  size_t status = parent->open(parent, &file, filename, file_mode, attributes);
  if (IS_EFI_ERROR(status)) return NULL;
  return file;
}

void close_efi_file(EfiFileHandle *file) {
  file->close(file);
}

// TODO: Report errors
size_t get_efi_file_info(EfiFileHandle *file, EfiFileInfo *out_info) {
  size_t size = sizeof(*out_info);
  return file->get_info(file, &EFI_FILE_INFO_GUID, &size, out_info);
}

size_t read_efi_file(EfiFileHandle *file, void *buffer, size_t limit) {
  size_t status = file->read(file, &limit, buffer);
  if (IS_EFI_ERROR(status)) return 0;
  return limit;
}

typedef struct {
  Sink sink;
  EfiSimpleTextOutputProtocol *out;
} EfiSink;

SYSV Error efi_sink_write(void *data, const void *buffer, uint32_t limit) {
  const uint8_t *bytes = buffer;
  EfiSink *this = (void *)data;
  for (uint32_t i = 0; i < limit; ++i) {
    char ch = bytes[i];
    if (ch == '\n') {
      this->out->output_string(this->out, L"\n\r");
    } else {
      this->out->output_string(this->out, (wchar_t[]){ch, 0});
    }
  }
  return OK;
}

EfiSink EFI_SINK = {
  .sink.write = efi_sink_write,
};

uint8_t compute_acpi_checksum(void *start, size_t length) {
  uint8_t *bytes = start;
  uint8_t sum = 0;
  for (size_t i = 0; i < length; ++i) sum += bytes[i];
  return sum;
}

// TODO: Allocate a buffer instead
uint8_t MEMORY_MAP[1024 * 32];
ALIGNED(16) uint8_t KERNEL_STACK[8 * 1024];

SYSV void jump_to_kernel(BootData *data, size_t entry_addr);
EFIAPI size_t efi_main(void *image_handle, EfiSystemTable *st) {
  st->con_out->clear_screen(st->con_out);
  EFI_SINK.out = st->con_out;
  LOG_SINK = &EFI_SINK.sink;
  size_t status = 0;

  log("Booting up");

  size_t page_count = 128;
  paddr_t pages_start;
  status = st->boot_services->allocate_pages(EFI_ALLOC_ANY_PAGES,
      EFI_BOOT_SERVICES_DATA, page_count, &pages_start);
  paddr_t initial_pages_start = pages_start;
  size_t initial_page_count = page_count;
  ASSERT(!status && "Failed to allocate memory");

  ASSERT(page_count > SIZEOF_IN_PAGES(BootData));
  
  BootData *data = (void *)pages_start;
  pages_start += SIZEOF_IN_PAGES(BootData) * PAGE_SIZE;
  page_count -= SIZEOF_IN_PAGES(BootData);

  size_t range_count = PAGE_SIZE / sizeof(PhysicalPageRange);
  DEBUGD(range_count);
  PhysicalPageRange *ranges = (void *)pages_start;
  pages_start += PAGE_SIZE;
  page_count--;

  data->alloc.capacity = range_count;
  data->alloc.ranges = ranges;

  push_free_pages(&data->alloc, pages_start, page_count);

  EfiGraphicsOutputProtocol *gop;

  status = st->boot_services->locate_protocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (void **)&gop);
  ASSERT(!status && "Failed to locate graphics protocol");

  EfiGraphicsModeInfo *info;
  size_t info_size;

  uint32_t mode_number = gop->mode == NULL ? 0 : gop->mode->mode_number;
  status = gop->query_mode(gop, mode_number, &info_size, &info);

  if (status == EFI_NOT_STARTED) status = gop->set_mode(gop, 0);
  ASSERT(!status && "Failed to set native mode");

  // TODO: Iterate through the modes and get their info
  // TODO: Use double buffering, don't read the video memory directly

  // TODO: Handle different formats
  data->fb = (Surface){
    .ptr = gop->mode->frame_buffer_base,
    .width = info->width,
    .height = info->height,
    .pitch = info->pixel_per_scan_line * 4,
  };

  fill_surface(&data->fb, 0xFF111111);
  FB_SINK.surface = data->fb;

  EfiLoadedImageProtocol *loaded_image;
  status = st->boot_services->handle_protocol(image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (void **)&loaded_image);
  ASSERT(!status && "Failed to get loaded image protocol");

#ifdef BUILD_DEBUG
  volatile uint64_t *gdb_marker = (void *)0x10000;
  volatile uint64_t *image_base_ptr = (void *)(0x10000 + 8);
  *image_base_ptr = (size_t)loaded_image->image_base;
  *gdb_marker = 0xDEADBEEF;
#endif

  EfiSimpleFileSystemProtocol *file_system;
  status = st->boot_services->handle_protocol(loaded_image->device_handle,
      &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (void **)&file_system);
  ASSERT(!status && "Failed to get simple file system protocol");

  EfiFileHandle *root;
  status = file_system->volume_open(file_system, &root);
  ASSERT(!status && "Failed to open root directory");

  for (size_t i = 0; i < st->number_of_table_entries; ++i) {
    EfiConfigTable *entry = &st->configuration_table[i];
    if (are_efi_guid_equal(&entry->vendor_guid, &EFI_ACPI2_TABLE_GUID)) {

    } else if (are_efi_guid_equal(&entry->vendor_guid, &EFI_ACPI_TABLE_GUID)) {
      // SOURCE: https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html#rsdp-structure

      Rsdp *rsdp = entry->vendor_table;
      ASSERT(are_strings_equal(rsdp->signature, "RSD PTR ", 8));
      ASSERT(compute_acpi_checksum(rsdp, sizeof(*rsdp)) == 0);
      ASSERT(rsdp->revision == 0);

      SdtHeader *rsdt = (void *)(size_t)rsdp->rsdt_address;
      // TODO: It can also be XSDT table instead, handle that case
      ASSERT(are_strings_equal(rsdt->signature, "RSDT", 4));
      ASSERT(compute_acpi_checksum(rsdt, rsdt->length) == 0);
      ASSERT(rsdt->revision == 1);

      uint32_t *entries = (void *)((uint8_t *)rsdt + sizeof(*rsdt));
      uint32_t entry_count = (rsdt->length - sizeof(*rsdt)) / 4;

      for (uint32_t i = 0; i < entry_count; ++i) {
        SdtHeader *header = (void *)(size_t)entries[i];
        // log("sdt entry: %S", 4, header->signature);
      }
    }
  }

  {
    EfiFileHandle *font_file = open_efi_file(root, L"font1.psf", EFI_FILE_MODE_READ, 0);
    ASSERT(font_file && "Failed to open font file");

    EfiFileInfo info;
    status = get_efi_file_info(font_file, &info);
    ASSERT(!status && "Failed to get file info");
    paddr_t buffer = alloc_pages2(&data->alloc, (info.file_size + PAGE_SIZE - 1) / PAGE_SIZE);
    read_efi_file(font_file, (void *)buffer, info.file_size);
    load_psf2_font(&data->font, (void *)buffer);
    FB_SINK.font = data->font;
    LOG_SINK = &FB_SINK.sink;
  }

  {
    EfiFileHandle *user_file = open_efi_file(root, L"user_main.elf", EFI_FILE_MODE_READ, 0);
    ASSERT(user_file && "Failed to open user program file");

    EfiFileInfo info;
    status = get_efi_file_info(user_file, &info);
    ASSERT(!status && "Failed to get file info");
    paddr_t buffer = alloc_pages2(&data->alloc, (info.file_size + PAGE_SIZE - 1) / PAGE_SIZE);
    read_efi_file(user_file, (void *)buffer, info.file_size);

    data->user_efi_file = (void *)buffer;
  }

  PageTable *pml4 = (void *)alloc_pages2(&data->alloc, 1);
  data->pml4 = pml4;
  memset(pml4, 0, PAGE_SIZE);

  map_pages(&data->alloc, pml4, initial_pages_start, initial_pages_start, initial_page_count * PAGE_SIZE,
      PAGE_BIT_WRITABLE | PAGE_BIT_PRESENT | PAGE_BIT_USER);
  map_pages(&data->alloc, pml4, (paddr_t)loaded_image->image_base, (vaddr_t)loaded_image->image_base,
      loaded_image->image_size, PAGE_BIT_WRITABLE | PAGE_BIT_PRESENT | PAGE_BIT_USER);
  map_pages(&data->alloc, pml4, (paddr_t)data->fb.ptr, (vaddr_t)data->fb.ptr,
      data->fb.height * data->fb.pitch,
      PAGE_BIT_WRITABLE | PAGE_BIT_USER | PAGE_BIT_PRESENT);

  EfiFileHandle *kernel_file = open_efi_file(root, L"kernel.elf", EFI_FILE_MODE_READ, 0);
  ASSERT(kernel_file && "Failed to open kernel file");

  EfiFileInfo file_info;
  status = get_efi_file_info(kernel_file, &file_info);
  ASSERT(!status && "Failed to get kernel file info");

  ASSERT(file_info.file_size);
  size_t size_in_pages = (file_info.file_size + PAGE_SIZE - 1) / PAGE_SIZE;
  paddr_t kernel_addr = alloc_pages2(&data->alloc, size_in_pages);

  size_t kernel_size = file_info.file_size;
  status = kernel_file->read(kernel_file, &kernel_size, (void *)kernel_addr);
  ASSERT(!status && "Failed to read kernel file");

  vaddr_t kernel_entry;
  load_elf_file(&data->alloc, pml4, (void *)kernel_addr, &kernel_entry);
  push_free_pages(&data->alloc, kernel_addr, size_in_pages);

  paddr_t kernel_stack = alloc_pages2(&data->alloc, 8);
  vaddr_t higher_half = 0xFFFF800000000000;
  map_pages(&data->alloc, pml4, kernel_stack, higher_half, 8 * PAGE_SIZE,
      PAGE_BIT_PRESENT | PAGE_BIT_WRITABLE | PAGE_BIT_USER);

  // TODO: Setup uart for logging and use it instead of the console
  // TODO: Make our own console?

  size_t map_key, descriptor_size;
  uint32_t descriptor_version;
  size_t memory_map_size = sizeof(MEMORY_MAP);
  size_t memory_descriptor_size;

  do {
    status = st->boot_services->get_memory_map(&memory_map_size, MEMORY_MAP, &map_key,
        &memory_descriptor_size, &descriptor_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
      st->con_out->output_string(st->con_out, L"Memory map buffer too small");
      TRAP();
    }
    ASSERT(!status && "Failed to get memory map");

    status = st->boot_services->exit_boot_services(image_handle, map_key);
    if (IS_EFI_ERROR(status)) {
      st->con_out->output_string(st->con_out, L"Failed to exit boot services\n\r");
    }
  } while (IS_EFI_ERROR(status));


  int count = memory_map_size / memory_descriptor_size;

  for (size_t offset = 0; offset < memory_map_size; offset += memory_descriptor_size) {
    EfiMemoryDescriptor *desc = (void *)(MEMORY_MAP + offset);
    // NOTE: We only handle one code section for now
    ASSERT(desc->type != EFI_LOADER_DATA);
    if (desc->type == EFI_CONVENTIONAL_MEMORY ) {
        // NOTE: We can't write to those areas before we set up our page tables
        // desc->type == EFI_BOOT_SERVICES_CODE || desc->type == EFI_BOOT_SERVICES_DATA) {
      push_free_pages(&data->alloc, desc->physical_start, desc->number_of_pages);
    }
  }

  paddr_t biggest_memory_addr = 0;
  size_t pages_total = 0;
  for (size_t i = 0; i < data->alloc.len; ++i) {
    PhysicalPageRange range = data->alloc.ranges[i];
    log("Memory %d addr=%X, pages=%d", i, range.start, range.page_count);
    pages_total += range.page_count;
    paddr_t end = range.start + range.page_count * PAGE_SIZE;
    if (end > biggest_memory_addr) biggest_memory_addr = end;
  }
  DEBUGD(pages_total);

  ASM("cli");

  paddr_t interrupt_stack = alloc_pages2(&data->alloc, 2);
  paddr_t interrupt_stack_top = interrupt_stack + 2 * PAGE_SIZE - 1;

  setup_gdt_and_tss((GdtEntry *)&data->gdt, &data->tss, (void *)interrupt_stack_top);
  setup_idt((InterruptDescriptor *)&data->idt);

  log("OK");

  ASM(
    "mov rsp, %0 \n"
    "call jump_to_kernel" ::
    "r"(kernel_stack + 8 * PAGE_SIZE - 1),
    "D"(data), "S"(kernel_entry)
   );
  UNREACHABLE();
}

SYSV void jump_to_kernel(BootData *data, size_t entry_addr) {
  log("Hello, kernel");

  ASM("mov cr3, %0" :: "r"((size_t)data->pml4 & PAGE_ADDR_MASK));

  ASM("jmp %0" :: "r"(entry_addr), "D"(data));

  for (;;) WFI();
}


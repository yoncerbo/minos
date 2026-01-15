#include "common.h"
#include "arch.h"
#include "efi.h"

EfiSimpleTextOutputProtocol *stderr;

void putchar_efi(char ch) {
  if (stderr) {
    if (ch == '\n') {
      stderr->output_string(stderr, L"\n\r");
    } else {
      stderr->output_string(stderr, (wchar_t[]){ch, 0});
    }
  }
}

Surface *log_surface;

void putchar_surface(char ch) {
  const int MARGIN = 16;
  static uint32_t x = MARGIN;
  static uint32_t y = MARGIN;

  if (x > log_surface->width - (8 + MARGIN)) {
    x = MARGIN;
    y += 16;
  }
  if (y > log_surface->height - (16 + MARGIN)) {
    return;
  }

  if (ch == '\n') {
    x = MARGIN;
    y += 16;
  } else {
    draw_char(log_surface, x, y, WHITE, ch);
    x += 8;
  }

}

// Source files
#include "drawing.c"
#include "print.c"

void assert_ok(size_t status, const wchar_t *error_message) {
  if (IS_EFI_ERROR(status)) {
    stderr->output_string(stderr, error_message);
    __builtin_trap();
  }
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
void get_efi_file_info(EfiFileHandle *file, EfiFileInfo *out_info) {
  size_t size = sizeof(*out_info);
  size_t status = file->get_info(file, &EFI_FILE_INFO_GUID, &size, out_info);
  assert_ok(status, L"Failed to read file info");
}

size_t read_efi_file(EfiFileHandle *file, void *buffer, size_t limit) {
  size_t status = file->read(file, &limit, buffer);
  if (IS_EFI_ERROR(status)) return 0;
  return limit;
}

EfiMemoryDescriptor MEMORY_MAP[1024];

size_t efi_setup(void *image_handle, EfiSystemTable *st, Surface *surface, size_t *memory_map_size) {
  stderr = st->con_out;
  putchar = putchar_efi;

  size_t status = 0;
  EfiGraphicsOutputProtocol *gop;

  status = st->boot_services->locate_protocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (void **)&gop);
  assert_ok(status, L"Failed to locate graphics protocol");

  EfiGraphicsModeInfo *info;
  size_t info_size;

  uint32_t mode_number = gop->mode == NULL ? 0 : gop->mode->mode_number;
  status = gop->query_mode(gop, mode_number, &info_size, &info);

  if (status == EFI_NOT_STARTED) status = gop->set_mode(gop, 0);
  assert_ok(status, L"Failed to get native mode");

  // TODO: Iterate through the modes and get their info
  // TODO: Use double buffering, don't read the video memory directly

  uint32_t color = 0xff111111;
  for (uint32_t y = 0; y < info->height; ++y) {
    for (uint32_t x = 0; x < info->width; ++x) {
      ((uint32_t *)gop->mode->frame_buffer_base)[y * gop->mode->info->pixel_per_scan_line + x] = color;
    }
  }

  // TODO: Handle different formats
  *surface = (Surface){
    .ptr = gop->mode->frame_buffer_base,
    .width = info->width,
    .height = info->height,
    .pitch = info->pixel_per_scan_line * 4,
  };
  fill_surface(surface, 0xff111111);

  EfiLoadedImageProtocol *loaded_image;
  status = st->boot_services->handle_protocol(image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (void **)&loaded_image);
  assert_ok(status, L"Failed to load image protocol");

  EfiSimpleFileSystemProtocol *file_system;
  status = st->boot_services->handle_protocol(loaded_image->device_handle,
      &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (void **)&file_system);
  assert_ok(status, L"Failed to load simple file system protocol");

  EfiFileHandle *root;
  status = file_system->volume_open(file_system, &root);
  assert_ok(status, L"Failed to open root file");

  // TODO: Functions to convert between asci and utf-16?
  // TODO: Setup uart for logging and use it instead of the console
  // TODO: Make our own console?

  size_t map_key, descriptor_size;
  uint32_t descriptor_version;
  *memory_map_size = sizeof(MEMORY_MAP);

  do {
    status = st->boot_services->get_memory_map(memory_map_size, MEMORY_MAP, &map_key,
        &descriptor_size, &descriptor_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
      stderr->output_string(stderr, L"Memory map buffer too small");
      TRAP();
    }
    assert_ok(status, L"Failed to get memory map");

    if (descriptor_size != sizeof(EfiMemoryDescriptor)) {
      stderr->output_string(stderr, L"ERROR: Bad memory descriptor size");
      DEBUGD(descriptor_size);
      DEBUGD(sizeof(EfiMemoryDescriptor));
      TRAP();
    }

    status = st->boot_services->exit_boot_services(image_handle, map_key);
    if (IS_EFI_ERROR(status)) {
      stderr->output_string(stderr, L"Failed to exit boot services\n\r");
    }
  } while (IS_EFI_ERROR(status));

  stderr = NULL;
  log_surface = surface;
  putchar = putchar_surface;

  return status;
}

size_t efi_main(void *image_handle, EfiSystemTable *st) {
  Surface surface;
  size_t memory_map_size;

  size_t status = efi_setup(image_handle, st, &surface, &memory_map_size);
  if (IS_EFI_ERROR(status)) return status;

  size_t best_page_count = 0;
  vaddr_t memory = 0;

  int count = memory_map_size / sizeof(MEMORY_MAP[0]);
  for (int i = 0; i < count; ++i) {
    EfiMemoryDescriptor *desc = &MEMORY_MAP[i];
    if (desc->type != EFI_CONVENTIONAL_MEMORY) continue;
    if (desc->number_of_pages <= best_page_count) continue;
    best_page_count = desc->number_of_pages;
    memory = desc->virtual_start;
  }

  ASSERT(best_page_count);
  LOG("Memory info: page_count=%d, start=0x%d\n", best_page_count, memory);

  // NOTE: Disable interrupts for setting up GDT and TSS
  ASM("cli");

  uint64_t tss_base = (uint64_t)&TSS;
  GDT_TABLE[5] = (GdtEntry)GDT_ENTRY(tss_base, sizeof(TSS) - 1, 0x89, 0xA);
  // *(uint32_t *)&GDT_TABLE[7] = (uint32_t)(tss_base >> 32);
  GDT_TABLE[6].limit0_15 = (tss_base >> 32) & 0xFFFF;
  GDT_TABLE[6].base0_15 = (tss_base >> 48) & 0xFFFF;

  GdtTablePtr gdt_table_ptr = { sizeof(GDT_TABLE) - 1, (uint64_t)&GDT_TABLE };
  load_gdt_table(&gdt_table_ptr);

  LOG("Setup finished\n", 0);

  for (;;) {
    WFI();
  }

}


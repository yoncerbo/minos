#include "common.h"
#include "efi.h"

void assert_ok(size_t status, EfiSimpleTextOutputProtocol *out, const wchar_t *error_message) {
  if (IS_EFI_ERROR(status)) {
    out->output_string(out, error_message);
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
  // ASSERT(status == 0);
}

size_t read_efi_file(EfiFileHandle *file, void *buffer, size_t limit) {
  size_t status = file->read(file, &limit, buffer);
  if (IS_EFI_ERROR(status)) return 0;
  return limit;
}


size_t efi_setup(void *image_handle, EfiSystemTable *st, Surface *surface, uint8_t *memory_map, size_t *memory_map_size, size_t *memory_descriptor_size) {
  size_t status = 0;
  EfiGraphicsOutputProtocol *gop;

  status = st->boot_services->locate_protocol(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, NULL, (void **)&gop);
  assert_ok(status, st->con_out, L"Failed to locate graphics protocol");

  EfiGraphicsModeInfo *info;
  size_t info_size;

  uint32_t mode_number = gop->mode == NULL ? 0 : gop->mode->mode_number;
  status = gop->query_mode(gop, mode_number, &info_size, &info);

  if (status == EFI_NOT_STARTED) status = gop->set_mode(gop, 0);
  assert_ok(status, st->con_out, L"Failed to get native mode");

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
  assert_ok(status, st->con_out, L"Failed to load image protocol");

  EfiSimpleFileSystemProtocol *file_system;
  status = st->boot_services->handle_protocol(loaded_image->device_handle,
      &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (void **)&file_system);
  assert_ok(status, st->con_out, L"Failed to load simple file system protocol");

  EfiFileHandle *root;
  status = file_system->volume_open(file_system, &root);
  assert_ok(status, st->con_out, L"Failed to open root file");

  // TODO: Functions to convert between asci and utf-16?
  // TODO: Setup uart for logging and use it instead of the console
  // TODO: Make our own console?

  size_t map_key, descriptor_size;
  uint32_t descriptor_version;

  do {
    status = st->boot_services->get_memory_map(memory_map_size, memory_map, &map_key,
        memory_descriptor_size, &descriptor_version);
    if (status == EFI_BUFFER_TOO_SMALL) {
      st->con_out->output_string(st->con_out, L"Memory map buffer too small");
      TRAP();
    }
    assert_ok(status, st->con_out, L"Failed to get memory map");

    status = st->boot_services->exit_boot_services(image_handle, map_key);
    if (IS_EFI_ERROR(status)) {
      st->con_out->output_string(st->con_out, L"Failed to exit boot services\n\r");
    }
  } while (IS_EFI_ERROR(status));

  return status;
}


#include "common.h"
#include "arch.h"
#include "efi.h"

void assert_ok(size_t status, EfiSimpleTextOutputProtocol *out, const wchar_t *error_message) {
  if (IS_EFI_ERROR(status)) {
    out->output_string(out, error_message);
    __builtin_trap();
  }
}

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

uint8_t EFI_STREAM_BUFFER[64];

EfiSink EFI_SINK = {
  .sink.write = efi_sink_write,
};

EfiSimpleTextOutputProtocol *efiout;

uint8_t compute_acpi_checksum(void *start, size_t length) {
  uint8_t *bytes = start;
  uint8_t sum = 0;
  for (size_t i = 0; i < length; ++i) sum += bytes[i];
  return sum;
}

// TODO: Allocate a buffer instead
uint8_t MEMORY_MAP[1024 * 32];
ALIGNED(16) uint8_t KERNEL_STACK[8 * 1024];

EFIAPI size_t efi_main(void *image_handle, EfiSystemTable *st) {
  EFI_SINK.out = st->con_out;
  LOG_BUFFER.sink = &EFI_SINK.sink;

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

  // TODO: Handle different formats
  BOOT_CONFIG.fb = (Surface){
    .ptr = gop->mode->frame_buffer_base,
    .width = info->width,
    .height = info->height,
    .pitch = info->pixel_per_scan_line * 4,
  };
  fill_surface(&BOOT_CONFIG.fb, 0xff111111);
  FB_SINK.surface = BOOT_CONFIG.fb;
  // LOG_BUFFER.sink = &FB_SINK.sink;
  LOG_BUFFER.sink = &FB_SINK.sink;

  EfiLoadedImageProtocol *loaded_image;
  status = st->boot_services->handle_protocol(image_handle, &EFI_LOADED_IMAGE_PROTOCOL_GUID, (void **)&loaded_image);
  assert_ok(status, st->con_out, L"Failed to load image protocol");
  BOOT_CONFIG.image_base = (size_t)loaded_image->image_base;

#ifdef BUILD_DEBUG
  volatile uint64_t *gdb_marker = (void *)0x10000;
  volatile uint64_t *image_base_ptr = (void *)(0x10000 + 8);
  *image_base_ptr = (size_t)loaded_image->image_base;
  *gdb_marker = 0xDEADBEEF;
#endif

  EfiSimpleFileSystemProtocol *file_system;
  status = st->boot_services->handle_protocol(loaded_image->device_handle,
      &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (void **)&file_system);
  assert_ok(status, st->con_out, L"Failed to load simple file system protocol");

  EfiFileHandle *root;
  status = file_system->volume_open(file_system, &root);
  assert_ok(status, st->con_out, L"Failed to open root file");

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
        log("sdt entry: %S", 4, header->signature);
      }
    }
  }

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
    assert_ok(status, st->con_out, L"Failed to get memory map");

    status = st->boot_services->exit_boot_services(image_handle, map_key);
    if (IS_EFI_ERROR(status)) {
      st->con_out->output_string(st->con_out, L"Failed to exit boot services\n\r");
    }
  } while (IS_EFI_ERROR(status));

  BOOT_CONFIG.memory_map_size = memory_map_size;
  BOOT_CONFIG.memory_descriptor_size = memory_descriptor_size;

  // NOTE: The current stack in in the UEFI boot data memory,
  // we need to set up a new stack
  uint8_t *stack_top = &KERNEL_STACK[sizeof(KERNEL_STACK) - 1];
  ASM("mov rsp, %0 \n jmp kernel_main" :: "g"(stack_top));
  UNREACHABLE();
}


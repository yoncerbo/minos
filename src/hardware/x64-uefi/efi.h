#ifndef INCLUDE_UEFI
#define INCLUDE_UEFI

#include "common.h"

// SOURCE: https://codeberg.org/bzt/posix-uefi/src/branch/master/uefi/uefi.h

typedef uint16_t wchar_t;

#define EFI_ERROR(a) ((a) | ~(((size_t)-1) >> 1))
#define IS_EFI_ERROR(a) ((a) & ~(((size_t)-1) >> 1))

#define EFI_SUCCESS 0
#define EFI_NOT_READY EFI_ERROR(6)
#define EFI_NOT_STARTED EFI_ERROR(19)
#define EFI_BUFFER_TOO_SMALL EFI_ERROR(5)

typedef struct {
  uint32_t data1;
  uint16_t data2, data3;
  uint8_t data4[8];
} EfiGuid;

const EfiGuid EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID =
  { 0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };

const EfiGuid EFI_LOADED_IMAGE_PROTOCOL_GUID =
  { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} };

const EfiGuid EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID =
  { 0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} };

const EfiGuid EFI_FILE_INFO_GUID =
  { 0x9576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} };

typedef enum {
  PixelRedGreenBlueReserved8BitPerColor,
  PixelBlueGreenRedReserved8BitPerColor,
  PixelBitMask,
  PixelBltOnly,
  PixelFormatMax
} EfiGraphicsPixelFormat;

typedef struct {
  uint32_t r, g, b, reserved;
} EfiGraphicsPixelBitmask;

typedef struct {
  uint32_t version, width, height;
  EfiGraphicsPixelFormat pixel_format;
  EfiGraphicsPixelBitmask pixel_info;
  uint32_t pixel_per_scan_line;
} EfiGraphicsModeInfo;

typedef struct {
  uint32_t max_mode, mode_number;
  EfiGraphicsModeInfo *info;
  size_t info_size;
  void *frame_buffer_base;
  size_t frame_buffer_size;
} EfiGraphicsMode;

typedef struct {
  size_t (*query_mode)(void *this, uint32_t mode_number, size_t *out_info_size, EfiGraphicsModeInfo **out_info);
  size_t (*set_mode)(void *this, uint32_t mode_number);
  size_t (*blt)(void *this);
  EfiGraphicsMode *mode;
} EfiGraphicsOutputProtocol;

typedef struct {
  uint8_t type, sub_type, length[2];
} EfiDevicePath;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
} EfiMemoryType;

typedef struct {
  uint32_t revision;
  void *parent_handle;
  void *system_table;
  void *device_handle;
  EfiDevicePath *filepath;
  void *reserved;
  uint32_t load_option_size;
  void *load_options;
  void *image_base;
  uint64_t image_size;
  EfiMemoryType image_code_type;
  EfiMemoryType image_data_type;
} EfiLoadedImageProtocol;

typedef struct {
  uint64_t signature;
  uint32_t revision, header_size, crc32, reserved;
} EfiTableHeader;

typedef struct {
  wchar_t scan_code;
  wchar_t unicode_char;
} EfiInputKey;

typedef struct {
  size_t (*reset)(void *this, bool extended_verification);
  size_t (*read_key_stroke)(void *this, EfiInputKey *key);
  void *wait_for_key;
} EfiSimpleTextInputProtocol;

typedef struct {
  uint32_t max_mode, mode, attribute, cursor_column, cursor_row;
  uint8_t cursor_visible;
} SimpleTextOutputMode;

typedef struct {
  size_t (*reset)(void *this, bool extended_verification);
  size_t (*output_string)(void *this, const wchar_t *string);
  void *test_string;
  size_t (*query_mode)(void *this, size_t mode_number, size_t *columns, size_t *rows);
  void *set_mode;
  size_t (*set_attribute)(void *this, size_t attribute);
  size_t (*clear_screen)(void *this);
  size_t (*set_cursor_position)(void *this, size_t columne, size_t row);
  size_t (*enable_cursor)(void *this, uint8_t visible);
  SimpleTextOutputMode *mode;
} EfiSimpleTextOutputProtocol;

typedef struct EfiFileHandle EfiFileHandle;

#define EFI_FILE_MODE_READ      0x0000000000000001
#define EFI_FILE_MODE_WRITE     0x0000000000000002
#define EFI_FILE_MODE_CREATE    0x8000000000000000

#define EFI_FILE_READ_ONLY      0x0000000000000001
#define EFI_FILE_HIDDEN         0x0000000000000002
#define EFI_FILE_SYSTEM         0x0000000000000004
#define EFI_FILE_RESERVED       0x0000000000000008
#define EFI_FILE_DIRECTORY      0x0000000000000010
#define EFI_FILE_ARCHIVE        0x0000000000000020
#define EFI_FILE_VALID_ATTR     0x0000000000000037

struct EfiFileHandle {
  uint64_t revision;
  size_t (*open)(EfiFileHandle *file, EfiFileHandle **out_new_file, const wchar_t *filename,
      uint64_t mode, uint64_t attributes);
  size_t (*close)(EfiFileHandle *file);
  size_t (*delete)(EfiFileHandle *file);
  size_t (*read)(EfiFileHandle *file, size_t *buffer_size, void *buffer);
  size_t (*write)(EfiFileHandle *file, size_t *buffer_size, void *buffer);
  size_t (*get_pos)(EfiFileHandle *file, uint64_t *out_position);
  size_t (*set_pos)(EfiFileHandle *file, uint64_t position);
  size_t (*get_info)(EfiFileHandle *file, const EfiGuid *info_type,
      size_t *buffer_size, void *buffer);
  size_t (*set_info)(EfiFileHandle *file, const EfiGuid *info_type,
      size_t *buffer_size, void *buffer);
  size_t (*flush)(EfiFileHandle *file);
};

typedef struct {
  uint16_t year;
  uint8_t month, day, hour, minute, second, padding1;
  uint32_t nanosecond;
  int16_t time_zone; // -1440 to 1440
  uint8_t daylight, padding2;
} EfiTime;

#define FILENAME_MAX 262

typedef struct {
  uint64_t size, file_size, physical_size;
  EfiTime create_time, last_access_time, modification_time;
  uint64_t attributes;
  wchar_t filename[FILENAME_MAX];
} EfiFileInfo;

typedef struct {
  uint64_t revision;
  size_t (*volume_open)(void *this, EfiFileHandle **out_root);
} EfiSimpleFileSystemProtocol;

typedef struct {
  uint32_t type;
  paddr_t physical_start;
  vaddr_t virtual_start;
  uint64_t number_of_pages, attributes;
  uint64_t _padding;
} EfiMemoryDescriptor;

typedef struct {
  EfiTableHeader header;

  void *raise_tpl;
  void *restore_tpl;

  void *allocate_pages;
  void *free_pages;
  size_t (*get_memory_map)(size_t *memory_map_size, EfiMemoryDescriptor *memory_map,
      size_t *map_key, size_t *descriptor_size, uint32_t *descriptor_version);
  void *allocate_pool;
  void *free_pool;

  void *create_event;
  void *set_timer;
  void *wait_for_event;
  void *signal_event;
  void *close_event;
  void *check_event;

  void *install_protocol_interface;
  void *reinstall_protocol_interface;
  void *uninstall_protocol_interface;
  size_t (*handle_protocol)(void *handle, const EfiGuid *protocol, void **out_interface);
  void *pc_handle_protocol;
  void *register_protocol_notify;
  void *locate_handle;
  void *locate_device_path;
  void *install_configuration_table;

  void *load_image;
  void *start_image;
  void *exit;
  void *unload_image;
  size_t (*exit_boot_services)(void *image_handle, size_t map_key);

  void *get_next_high_monotonic_count;
  void *stall;
  void *set_watchdog_timer;
  
  void *connect_controller;
  void *disconnect_controller;

  void *open_protocol;
  void *close_protocol;
  void *open_protocol_information;

  void *protocols_per_handle;
  void *locate_handle_buffer;
  size_t (*locate_protocol)(const EfiGuid *protocol, void *registration, void **out_interface);
  void *install_multiple_protocol_interfaces;
  void *uninstall_multiple_protocol_interfaces;

  void *calculate_crc32;

} EfiBootServices;

typedef struct {
  EfiTableHeader header;
  void *firmware_vendor;
  uint32_t firmware_revision;
  void *console_in_handle;
  EfiSimpleTextInputProtocol *con_in;
  void *console_out_handle;
  EfiSimpleTextOutputProtocol *con_out;
  void *standard_error_handle;
  EfiSimpleTextOutputProtocol *stderr;
  void *runtime_services;
  EfiBootServices *boot_services;
  size_t number_of_table_entries;
  void *configuration_table;
} EfiSystemTable;


#endif

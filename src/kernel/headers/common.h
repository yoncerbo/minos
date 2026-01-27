#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

#include "cmn/lib.h"
#include "interfaces/input.h"

typedef size_t paddr_t;
typedef size_t vaddr_t;

typedef enum {
  IS_CAPSLOCK = 1 << 0,
  IS_LSHIFT = 1 << 1,
  IS_RSHIFT = 1 << 2,
} TextInputFlags;

typedef struct {
  uint32_t flags;
} TextInputState;

extern char PSF_FONT_START[];

const uint16_t PSF1_MAGIC = 0x0436;
const uint32_t PSF2_MAGIC = 0x864ab572;

typedef struct {
  uint16_t magic;
  uint8_t font_mode;
  uint8_t character_size;
} Psf1Header;

typedef struct {
  uint32_t magic, version, header_size, flags;
  uint32_t glyph_count, bytes_per_glyph;
  uint32_t height_pixels, width_pixels;
} Psf2Header;

// src/drawing.c
typedef struct {
  uint32_t *ptr;
  uint32_t width, height, pitch;
} Surface;

typedef struct {
  uint32_t width, height;
  uint32_t glyph_size;
  uint8_t *glyphs;
} Font;

typedef struct {
  Sink sink;
  TextInputState input_state;
  Surface surface;
  Font font;
  uint32_t bg, fg;
  uint32_t x, y;
  uint32_t line_spacing;
  uint32_t buffer_pos;
  char command_buffer[256];
} Console;

void clear_console(Console *c);
void push_console_input_event(Console *c, InputEvent event);

const uint32_t WHITE = 0xFFFFFFFF;
const uint32_t BLACK = bswap32(0x000000FF);
const uint32_t RED = bswap32(0xFF0000FF);
const uint32_t BLUE = bswap32(0x0000FFFF);
const uint32_t GREEN = bswap32(0x00FF00FF);

void draw_char(Surface *surface, int x, int y, uint32_t color, uint8_t character);
void draw_char2(Surface *surface, Font *font, int x, int y, uint32_t color, uint8_t character);
void draw_char3(Surface *surface, Font *font, int x, int y, uint32_t fg, uint32_t bg, uint8_t character);
// Draws until limit or null byte
void draw_line(Surface *surface, int x, int y, uint32_t color, const char *str, uint32_t limit);
void fill_surface(Surface *surface, uint32_t color);
void load_psf2_font(Font *out_font, void *file);

// NOTE : in memory 0x7F "ELF"
#define ELF_MAGIC 0x464C457F

#define ELF_CLASS32 1
#define ELF_CLASS64 2
#define ELF_ENDIAN_LITTLE 1
#define ELF_ENDIAN_BIG 2
#define ELF_OS_ABI_SYSTEM_V 0

#define ELF_TYPE_RELOCATABLE 1
#define ELF_TYPE_EXECUTABLE 2

#define ELF_ISA_X86_64 0x3E

// SOURCE: man elf, https://wiki.osdev.org/ELF#ELF_Header
typedef struct PACKED {
  uint32_t magic;
  uint8_t class; // 0: invalid, 1: 32bit, 2: 64bit
  uint8_t endianness; // 0: invalid, 1: little endian, 2: big endian
  uint8_t header_version; // 0: invalid, 1: current
  uint8_t os_abi; // 0: system v
  uint8_t abi_version;
  uint8_t padding[7];
  uint16_t type; // 1: relocatable, 2: executable, 3: shared, 4: core
  uint16_t isa;
  uint32_t version; // 1

  uint64_t program_entry_addr;
  uint64_t program_table_offset;
  uint64_t section_table_offset;
  uint32_t flags;

  uint16_t header_size;
  uint16_t program_header_size;
  uint16_t program_table_entry_count;
  uint16_t section_header_size;
  uint16_t section_table_entry_count;
  uint16_t string_table_section_index;
} ElfHeader64;

CASSERT(sizeof(ElfHeader64) == 64);

typedef struct PACKED {
  uint32_t type;
  uint32_t flags;
  uint64_t file_offset;
  uint64_t virtual_addr;
  uint64_t physical_addr;
  uint64_t size_in_file;
  uint64_t size_in_memory; // at least size_in_file
  uint64_t alignment;
} ElfProgramHeader64;

typedef enum {
  ELF_PROG_NULL = 0,
  ELF_PROG_LOAD = 1,
  ELF_PROG_DYNAMIC = 2,
  ELF_PROG_INTERPRETER = 3,
  ELF_PROG_NOTE = 4,
} ElfProgramHeaderType;

typedef enum {
  ELF_PROG_EXECUTABLE = 1,
  ELF_PROG_WRITEBALE = 2,
  ELF_PROG_READABLE = 4,
} ElfProgramHeaderFlags;

#include "kernel/interfaces/gpu.h"
#include "kernel/interfaces/blk.h"
#include "kernel/interfaces/input.h"

// src/kernel.c
typedef struct {
  GpuDev *gpu;
  BlkDev *blk_devices;
  InputDev *input_devices;
  uint32_t blk_devices_count;
  uint32_t input_devices_count;
  
} Hardware;
void kernel_init(Hardware *hw);
void kernel_update(Hardware *hw);

#endif

#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

#include "cmn/lib.h"

typedef size_t paddr_t;
typedef size_t vaddr_t;

typedef enum {
  OK = 0,
  ERR_OUT_OF_SPACE,
} Error;

typedef struct Sink {
  Error (SYSV *write)(void *this, const void *buffer, uint32_t limit);
} Sink;

typedef struct {
  uint8_t *ptr;
  uint32_t capacity;
  uint32_t position;
} Buffer;

Error write(Sink *sink, const void *buffer, uint32_t limit);
Error write_str(Sink *sink, Str str);
Error writeb(Buffer *buffer, Sink *sink, const void *content, uint32_t limit);
Error write_strb(Buffer *buffer, Sink *sink, Str str);
void putcharb(Buffer *buffer, Sink *sink, char ch);
void vprintb(Buffer *buffer, Sink *sink, const char *format, va_list vargs);
void flush_buffer(Buffer *buffer, Sink *sink);
void prints(Sink *sink, const char *format, ...);
void log(const char *format, ...);

Sink *LOG_SINK = NULL;

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

const uint32_t WHITE = 0xFFFFFFFF;
const uint32_t BLACK = bswap32(0x000000FF);
const uint32_t RED = bswap32(0xFF0000FF);
const uint32_t BLUE = bswap32(0x0000FFFF);
const uint32_t GREEN = bswap32(0x00FF00FF);

void draw_char(Surface *surface, int x, int y, uint32_t color, uint8_t character);
void draw_char2(Surface *surface, Font *font, int x, int y, uint32_t color, uint8_t character);
// Draws until limit or null byte
void draw_line(Surface *surface, int x, int y, uint32_t color, const char *str, uint32_t limit);
void fill_surface(Surface *surface, uint32_t color);
void load_psf2_font(Font *out_font, void *file);

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

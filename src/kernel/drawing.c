#include "common.h"
#include "font.h"

void draw_char(Surface *surface, int x, int y, uint32_t color, uint8_t character) {
  // TODO: Consider using integer vector for position
  for (uint32_t i = 0; i < 8; ++i) {
    uint32_t *row = (uint32_t *)((uint8_t *)surface->ptr + surface->pitch * (y + i) + x * 4);
    // TODO: Bounds checking
    for (uint32_t j = 0; j < 8; ++j, ++row) {
      if (font8x8_basic[character][i] & (1 << j)) *row = color;
    }
  }
}

void draw_char2(Surface *surface, Font *font, int x, int y, uint32_t color, uint8_t character) {
  // TODO: Clip the character if outside of bounds
  for (uint32_t i = 0; i < font->height; ++i) {
    uint32_t *row = (uint32_t *)((uint8_t *)surface->ptr + surface->pitch * (y + i) + x * 4);
    // TODO: Bounds checking
    for (uint32_t j = 0; j < font->width; ++j, ++row) {
      if (font->glyphs[character * font->glyph_size + i] & (1 << (8 - j))) *row = color;
    }
  }
}

const uint32_t FONT_HEIGHT = 12;

void draw_line(Surface *surface, int x, int y, uint32_t color, const char *str, uint32_t limit) {
  for (uint32_t i = 0; i < limit && str[i]; ++i) {
    draw_char(surface, x + i * 8, y, color, str[i]);
  }
}

void fill_surface(Surface *surface, uint32_t color) {
  for (uint32_t y = 0; y < surface->height; ++y) {
    uint32_t *row = (uint32_t *)((uint8_t *)surface->ptr + y * surface->pitch);
    for (uint32_t x = 0; x < surface->width; ++x) {
      row[x] = color;
    }
  }
}

void load_psf2_font(Font *out_font, void *file) {
  Psf2Header *psf = (void *)file;
  ASSERT(psf->magic == PSF2_MAGIC);
  ASSERT(psf->version == 0);
  ASSERT(psf->header_size == 32);
  ASSERT(psf->glyph_count >= 128);

  ASSERT(psf->width_pixels <= 8 && "Font width greater than 8 not supported");

  out_font->width = psf->width_pixels;
  out_font->height = psf->height_pixels;
  out_font->glyph_size = psf->bytes_per_glyph;
  out_font->glyphs = (uint8_t *)file + 32;
}

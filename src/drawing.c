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

const uint32_t FONT_HEIGHT = 12;

void draw_line(Surface *surface, int x, int y, uint32_t color, const char *str, uint32_t limit) {
  for (uint32_t i = 0; i < limit && str[i]; ++i) {
    draw_char(surface, x + i * 8, y, color, str[i]);
  }
}

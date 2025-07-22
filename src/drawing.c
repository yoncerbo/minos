#include "common.h"
#include "virtio_gpu.h"
#include "font.h"
#include "drawing.h"

const uint32_t BLACK = bswap32(0x000000ff);
const uint32_t WHITE = 0xffffffff;

void draw_char(uint32_t *fb, uint32_t x, uint32_t y, uint32_t color, uint8_t ch) {
  for (uint32_t i = 0; i < 8; ++i) {
    for (uint32_t j = 0; j < 8; ++j) {
      if (font8x8_basic[ch][i] & (1 << j)) {
        fb[(y + i) * DISPLAY_WIDTH + x + j] = color;
      }
    }
  }
}

// Draws until limit or null byte
void draw_chars(uint32_t *fb, uint32_t x, uint32_t y, uint32_t color, const char *str, uint32_t limit) {
  for (uint32_t i = 0; i < limit && str[i]; ++i) {
    draw_char(fb, x + 8 * i, y, color, str[i]);
  }
}

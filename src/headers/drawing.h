#ifndef INCLUDE_DRAWING
#define INCLUDE_DRAWING

#include "common.h"

void draw_char(uint32_t *fb, uint32_t x, uint32_t y, uint32_t color, uint8_t ch);
void draw_chars(uint32_t *fb, uint32_t x, uint32_t y, uint32_t color, const char *str, uint32_t limit);

#endif

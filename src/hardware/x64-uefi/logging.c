#include "common.h"
#include "arch.h"

SYSV Error qemu_debugcon_write(void *data, const void *buffer, uint32_t limit) {
  Sink *this = data;
  const uint8_t *bytes = buffer;
  for (size_t i = 0; i < limit; ++i) {
    WRITE_PORT(0xE9, bytes[i]);
  }
  return OK;
}

Sink QEMU_DEBUGCON_SINK = {
  .write = qemu_debugcon_write,
};

const uint32_t MARGIN = 16;

SYSV Error fb_sink_write(void *data, const void *buffer, uint32_t limit) {
  const uint8_t *bytes = buffer;
  FbSink *fb = (void *)data;

  for (uint32_t i = 0; i < limit; ++i) {
    char ch = bytes[i];
    if (fb->x >= fb->surface.width - fb->font_width) {
      fb->x = MARGIN;
      fb->y += fb->font_height;
    }

    if (fb->y >= fb->surface.height - fb->font_height) {
      return OK;
    }

    if (ch == '\n') {
      fb->x = MARGIN;
      fb->y += fb->font_height;
    } else {
      draw_char(&fb->surface, fb->x, fb->y, WHITE, ch);
      fb->x += fb->font_width;
    }
  }
  return OK;
}

FbSink FB_SINK = {
  .sink.write = fb_sink_write,
  .x = MARGIN,
  .y = MARGIN,
  .font_width = 8,
  .font_height = 16,
};

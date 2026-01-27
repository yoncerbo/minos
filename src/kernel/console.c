#include "cmn/lib.h"
#include "common.h"
#include "interfaces/input.h"

void console_write_char(Console *c, char ch) {
  if (ch == '\n') {
    c->x = 0;
    c->y += c->font.height + c->line_spacing;
    return;
  }

  if (c->x + c->font.width > c->surface.width) {
    c->x = 0;
    c->y += c->font.height + c->line_spacing;
  }

  if (c->y >= c->surface.height - c->font.height) {
    return;
  }

  draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, ch);
  c->x += c->font.width;
}

Error console_write(void *data, const void *buffer, uint32_t limit) {
  Console *c = data;
  const char *chars = buffer;
  for (uint32_t i = 0; i < limit; ++i) {
    console_write_char(c, chars[i]);
  }
  return OK;
}

void clear_console(Console *c) {
  fill_surface(&c->surface, c->bg);
  console_write_char(c, '>');
  console_write_char(c, ' ');
  draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, '_');
}

void push_console_input_event(Console *c, InputEvent event) {
  if (event.type != EV_KEY || !event.value) return;
  // TODO: Add simple shortcuts: clear screen, clear command line, etc.

  switch (event.code) {
    case KEY_ENTER: {
      draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, ' ');
      c->x = 0;
      c->y += c->font.height;

      // TODO: Stripping the command
      if (c->buffer_pos == 4 && are_strings_equal("ping", c->command_buffer, 4)) {
        prints(&c->sink, "pong\n");
      } else {
        prints(&c->sink, "Unknown command: '%S'\n", c->buffer_pos, c->command_buffer);
      }
      c->buffer_pos = 0;

      prints(&c->sink, "> ");
    } break;
    case KEY_BACKSPACE: {
      if (c->buffer_pos == 0) break;
      c->buffer_pos--;
      draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, ' ');
      if (c->x > c->font.width) {
        c->x -= c->font.width;
      } else if (c->y > c->font.height + c->line_spacing) {
        c->x = 0;
        c->y -= c->font.height + c->line_spacing;
      }
      draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, '_');
    } break;
    default: {
      char ch = push_input_event(&c->input_state, event);
      if (ch && c->buffer_pos < 256) {
        console_write_char(c, ch);
        c->command_buffer[c->buffer_pos++] = ch;
      }
    } break;
  }
  draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, '_');
}

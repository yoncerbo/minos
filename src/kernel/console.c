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
  // TODO: Clear only damaged areas
  c->x = 0;
  c->y = 0;
  fill_surface(&c->surface, c->bg);
  prints(&c->sink, "> ");
  draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, '_');
}

enum {
  KEY_RELEASED = 0,
  KEY_PRESSED = 1,
};

void process_console_command(Console *c) {
  uint32_t start;
  for (start = 0; start < c->buffer_pos; ++start) {
    if (c->command_buffer[start] != ' ') break;
  }
  uint32_t last;
  for (last = c->buffer_pos - 1; last > start; --last) {
    if (c->command_buffer[last] != ' ') break;
  }
  uint32_t len = last + 1 - start;

  if (len == 4 && are_strings_equal("ping", &c->command_buffer[start], 4)) {
    prints(&c->sink, "pong\n");
  } else {
    prints(&c->sink, "Unknown command: '%S'\n", len, &c->command_buffer[start]);
  }
}

void push_console_input_event(Console *c, InputEvent event) {
  if (event.type != EV_KEY) return;

  bool is_ctrl = (c->input_state.modifiers & MOD_LCTRL) || (c->input_state.modifiers & MOD_RCTRL);
  if (event.value == KEY_PRESSED && is_ctrl) {
    switch (event.code) {
      case KEY_C: {
        c->buffer_pos = 0;
        draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, ' ');
        c->x = 0;
        c->y += c->font.height;
        prints(&c->sink, "> ");
      } return;
      case KEY_L: {
        clear_console(c);
        for (uint32_t i = 0; i < c->buffer_pos; ++i) {
          console_write_char(c, c->command_buffer[i]);
        }
      } return;
      default: break;
    }
  }

  switch (event.code) {
    case KEY_ENTER: {
      if (event.value != KEY_RELEASED) break;
      draw_char3(&c->surface, &c->font, c->x, c->y, c->fg, c->bg, ' ');
      c->x = 0;
      c->y += c->font.height;
      process_console_command(c);
      c->buffer_pos = 0;
      prints(&c->sink, "> ");
    } break;
    case KEY_BACKSPACE: {
      if (c->buffer_pos == 0 || event.value != KEY_PRESSED) break;
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

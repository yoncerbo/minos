#include "common.h"

void printf(const char *fmt, ...) {
  va_list vargs;
  va_start(vargs, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      putchar(*fmt++);
      continue;
    }

    switch (*++fmt) {
      case 0:
        goto while_end;
      case '%':
        putchar('%');
        break;
      case 'c': {
        char ch = va_arg(vargs, int);
        putchar(ch);
      } break;
      case 's': {
        const char *s = va_arg(vargs, const char *);
        while (*s) putchar(*s++);
      } break;
      case 'x': {
        uint32_t v = va_arg(vargs, uint32_t);
        for (int i = 7; i >= 0; i--) {
          uint32_t nibble = (v >> (i * 4)) & 0xf;
          putchar("0123456789ABCDEF"[nibble]);
        }
      } break;
      case 'X': {
        uint64_t v = va_arg(vargs, uint64_t);
        for (int i = 15; i >= 0; i--) {
          uint64_t nibble = (v >> (i * 4)) & 0xf;
          putchar("0123456789ABCDEF"[nibble]);
        }
      } break;
      case 'd': {
        isize_t value = va_arg(vargs, isize_t);
        size_t  magnitude = value;
        if (value < 0) {
          putchar('-');
          magnitude = -magnitude;
        }

        size_t divisor = 1;
        while (magnitude / divisor > 9) divisor *= 10;
        while (divisor > 0) {
          putchar('0' + magnitude / divisor);
          magnitude %= divisor;
          divisor /= 10;
        }
      } break;
      default: break;
    }
    fmt++;
  }
while_end:

  va_end(vargs);
}


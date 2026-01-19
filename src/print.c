#include "common.h"

Error write(Sink *sink, const void *buffer, uint32_t limit) {
  return sink->write(sink, buffer, limit);
}

Error write_str(Sink *sink, Str str) {
  return sink->write(sink, str.ptr, str.len);
}

Error write_buffered(BufferedSink *sink, const void *buffer, uint32_t limit) {
  ASSERT(sink);
  ASSERT(sink->sink);
  ASSERT(buffer);

  const uint8_t *bytes = buffer;
  uint32_t pos = 0;
  while (pos < limit) {
    uint32_t stream_limit = sink->capacity - sink->position;
    uint32_t buffer_limit = limit - pos;
    if (buffer_limit < stream_limit) {
      memcpy(&sink->ptr[sink->position], &bytes[pos], buffer_limit);
      pos = limit;
      sink->position += buffer_limit;
    } else {
      memcpy(&sink->ptr[sink->position], &bytes[pos], stream_limit);
      pos += stream_limit;
      sink->position = sink->capacity;
      Error err = sink->sink->write(sink->sink, sink->ptr, sink->position);
      if (err) return err;
    }
  }
  return OK;
}


Error write_str_buffered(BufferedSink *sink, Str str) {
  return write_buffered(sink, str.ptr, str.len);
}

void putchar_buffer(BufferedSink *buffer, char ch) {
  if (buffer->position >= buffer->capacity) {
    Error err = buffer->sink->write(buffer->sink, buffer->ptr, buffer->capacity);
    ASSERT(err == OK);
    buffer->position = 0;
  }
  buffer->ptr[buffer->position++] = ch;
}

void vprints(BufferedSink *buffer, const char *format, va_list vargs) {
  if (buffer == NULL) return;
  while (*format) {
    if (*format != '%') {
      putchar_buffer(buffer, *format++);
      continue;
    }
    ++format;
    switch (*format) {
      case '%': {
        putchar_buffer(buffer, '%');
        ++format;
      } break;
      case 'c': {
        putchar_buffer(buffer, va_arg(vargs, int));
        ++format;
      } break;
      case 's': {
        const char *s = va_arg(vargs, const char *);
        ASSERT(s);
        while (*s) putchar_buffer(buffer, *s++);
        ++format;
      } break;
      case 'S': {
        size_t limit = va_arg(vargs, size_t);
        const char *s = va_arg(vargs, const char *);
        ASSERT(s);
        for (size_t i = 0; i < limit && *s; ++i, ++s) putchar_buffer(buffer, *s);
        ++format;
      } break;
      case 'd': {
        isize_t value = va_arg(vargs, isize_t);
        size_t  magnitude = value;
        if (value < 0) {
          putchar_buffer(buffer, '-');
          magnitude = -magnitude;
        }

        size_t divisor = 1;
        while (magnitude / divisor > 9) divisor *= 10;
        while (divisor > 0) {
          putchar_buffer(buffer, '0' + magnitude / divisor);
          magnitude %= divisor;
          divisor /= 10;
        }
        ++format;
      } break;
      case 'X':
        putchar_buffer(buffer, '0');
        putchar_buffer(buffer, 'x');
        __attribute__((fallthrough));
      case 'x': {
        size_t value = va_arg(vargs, size_t);
        size_t  magnitude = value;

        size_t divisor = 1;
        while (magnitude / divisor > 15) divisor *= 16;
        while (divisor > 0) {
          putchar_buffer(buffer, "0123456789ABCDEF"[magnitude / divisor]);
          magnitude %= divisor;
          divisor /= 16;
        }
        format++;
      } break;
      default: break;
    }
  }
}

void prints(Sink *sink, const char *format, ...) {
  uint8_t stack_buffer[32];
  BufferedSink buffer = {
    .ptr = stack_buffer,
    .capacity = 32,
    .sink = sink,
  };
  va_list vargs;
  va_start(vargs, format);
  vprints(&buffer, format, vargs);
  va_end(vargs);
  sink->write(sink, buffer.ptr, buffer.position);
}

void log(const char *format, ...) {
  write_str_buffered(&LOG_BUFFER, STR("[LOG] "));
  va_list vargs;
  va_start(vargs, format);
  vprints(&LOG_BUFFER, format, vargs);
  va_end(vargs);
  putchar_buffer(&LOG_BUFFER, 10);
  LOG_BUFFER.sink->write(LOG_BUFFER.sink, LOG_BUFFER.ptr, LOG_BUFFER.position);
  LOG_BUFFER.position = 0;
}

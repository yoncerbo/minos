#include "cmn/lib.h"

Error write(Sink *sink, const void *buffer, uint32_t limit) {
  return sink->write(sink, buffer, limit);
}

Error write_str(Sink *sink, Str str) {
  return sink->write(sink, str.ptr, str.len);
}

Error writeb(Buffer *buffer, Sink *sink, const void *content, uint32_t limit) {
  ASSERT(buffer);
  ASSERT(sink);
  ASSERT(content);

  const uint8_t *bytes = content;
  uint32_t pos = 0;
  while (pos < limit) {
    uint32_t stream_limit = buffer->capacity - buffer->position;
    uint32_t content_limit = limit - pos;
    if (content_limit < stream_limit) {
      memcpy(&buffer->ptr[buffer->position], &bytes[pos], content_limit);
      pos = limit;
      buffer->position += content_limit;
    } else {
      memcpy(&buffer->ptr[buffer->position], &bytes[pos], stream_limit);
      pos += stream_limit;
      buffer->position = buffer->capacity;
      Error err = write(sink, buffer->ptr, buffer->position);
      if (err) return err;
    }
  }
  return OK;
}


Error write_strb(Buffer *buffer, Sink *sink, Str str) {
  return writeb(buffer, sink, str.ptr, str.len);
}

void putcharb(Buffer *buffer, Sink *sink, char ch) {
  if (buffer->position >= buffer->capacity) {
    Error err = sink->write(sink, buffer->ptr, buffer->capacity);
    ASSERT(err == OK);
    buffer->position = 0;
  }
  buffer->ptr[buffer->position++] = ch;
}

void vprintb(Buffer *buffer, Sink *sink, const char *format, va_list vargs) {
  if (buffer == NULL) return;
  while (*format) {
    if (*format != '%') {
      putcharb(buffer, sink, *format++);
      continue;
    }
    ++format;
    switch (*format) {
      case '%': {
        putcharb(buffer, sink, '%');
        ++format;
      } break;
      case 'c': {
        putcharb(buffer, sink, va_arg(vargs, int));
        ++format;
      } break;
      case 's': {
        const char *s = va_arg(vargs, const char *);
        ASSERT(s);
        while (*s) putcharb(buffer, sink, *s++);
        ++format;
      } break;
      case 'S': {
        size_t limit = va_arg(vargs, size_t);
        const char *s = va_arg(vargs, const char *);
        ASSERT(s);
        for (size_t i = 0; i < limit && *s; ++i, ++s) putcharb(buffer, sink, *s);
        ++format;
      } break;
      case 'd': {
        isize_t value = va_arg(vargs, isize_t);
        size_t  magnitude = value;
        if (value < 0) {
          putcharb(buffer, sink, '-');
          magnitude = -magnitude;
        }

        size_t divisor = 1;
        while (magnitude / divisor > 9) divisor *= 10;
        while (divisor > 0) {
          putcharb(buffer, sink, '0' + magnitude / divisor);
          magnitude %= divisor;
          divisor /= 10;
        }
        ++format;
      } break;
      case 'X':
        putcharb(buffer, sink, '0');
        putcharb(buffer, sink, 'x');
        __attribute__((fallthrough));
      case 'x': {
        size_t value = va_arg(vargs, size_t);
        size_t  magnitude = value;

        size_t divisor = 1;
        while (magnitude / divisor > 15) divisor *= 16;
        while (divisor > 0) {
          putcharb(buffer, sink, "0123456789ABCDEF"[magnitude / divisor]);
          magnitude %= divisor;
          divisor /= 16;
        }
        format++;
      } break;
      default: break;
    }
  }
}

void flush_buffer(Buffer *buffer, Sink *sink) {
  write(sink, buffer->ptr, buffer->position);
  buffer->position = 0;
}

void prints(Sink *sink, const char *format, ...) {
  uint8_t stack_buffer[32];
  Buffer buffer = { stack_buffer, 32, 0};
  va_list vargs;
  va_start(vargs, format);
  vprintb(&buffer, sink, format, vargs);
  va_end(vargs);
  sink->write(sink, buffer.ptr, buffer.position);
}

uint8_t LOG_BUFFER_INNER[64];
Buffer LOG_BUFFER = {LOG_BUFFER_INNER, 64, 0};

void log(const char *format, ...) {
  write_strb(&LOG_BUFFER, LOG_SINK, STR("[LOG] "));
  va_list vargs;
  va_start(vargs, format);
  vprintb(&LOG_BUFFER, LOG_SINK, format, vargs);
  va_end(vargs);
  putcharb(&LOG_BUFFER, LOG_SINK, 10);
  flush_buffer(&LOG_BUFFER, LOG_SINK);
}

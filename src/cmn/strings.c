#include "cmn/lib.h"

void *memcpy(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;
  for (uint32_t i = 0; i < n; ++i) d[i] = s[i];
  return dest;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;
  while (n--) *p++ = c;
  return s;
}

bool are_strings_equal(const char *s1, const char *s2, size_t limit) {
  for (size_t i = 0; i < limit; ++i) if (s1[i] != s2[i]) return false;
  return true;
}

inline uint32_t __bswapsi2(uint32_t u) {
  return ((((u)&0xff000000) >> 24) |
           (((u)&0x00ff0000) >> 8)  |
           (((u)&0x0000ff00) << 8)  |
           (((u)&0x000000ff) << 24));
}

const char *strip_string(const char *str, uint32_t limit, uint32_t *out_len) {
  uint32_t start = 0;
  for (; start < limit && str[start] && str[start] == ' '; ++start);
  uint32_t end = limit;
  while (end && str[--end] == ' ');
  *out_len = end - start + 1;
  return &str[start];
}

#include "common.h"

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

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t i = 0;
  for (; i < n && s1[i] == s2[i]; ++i);
  return (uint8_t)s1[i] - (uint8_t)s2[i];
}

inline uint32_t __bswapsi2(uint32_t u) {
  return ((((u)&0xff000000) >> 24) |
           (((u)&0x00ff0000) >> 8)  |
           (((u)&0x0000ff00) << 8)  |
           (((u)&0x000000ff) << 24));
}


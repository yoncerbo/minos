#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

#include "input_codes.h"

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

#define CASSERT(predicate) typedef char assertion_failed##__LINE__[(predicate) ? 1 : -1]

CASSERT(sizeof(int8_t) == 1);
CASSERT(sizeof(int16_t) == 2);
CASSERT(sizeof(int32_t) == 4);
CASSERT(sizeof(int64_t) == 8);
CASSERT(sizeof(uint8_t) == 1);
CASSERT(sizeof(uint16_t) == 2);
CASSERT(sizeof(uint32_t) == 4);
CASSERT(sizeof(uint64_t) == 8);

#if defined ARCH_X64
  typedef uint64_t size_t;
#elif defined ARCH_RV32
  typedef uint32_t size_t;
#else
#error "Unknown architecture"
#endif

typedef size_t paddr_t;
typedef size_t vaddr_t;
typedef _Bool bool;

typedef struct {
  const char *ptr;
  uint32_t len;
} Str;

#define NULL 0
#define true 1
#define false 0

#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
#define bswap16(n) __builtin_bswap16(n)
#define bswap32(n) __builtin_bswap32(n)

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x) 
#define CSTR_LEN(str) (sizeof(str) - 1)
#define STR(str) ((Str){ (str), CSTR_LEN(str) })

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define UPPER_BOUND(a, b) MIN(a, b)
#define LOWER_BOUND(a, b) MAX(a, b)

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
uint32_t __bswapsi2(uint32_t u);

// src/drawing.c
typedef struct {
  uint32_t *ptr;
  uint32_t width, height, pitch;
} Surface;

const uint32_t WHITE = 0xFFFFFFFF;
const uint32_t BLACK = bswap32(0x000000FF);
const uint32_t RED = bswap32(0xFF0000FF);
const uint32_t BLUE = bswap32(0x0000FFFF);
const uint32_t GREEN = bswap32(0x00FF00FF);

void draw_char(Surface *surface, int x, int y, uint32_t color, uint8_t character);
// Draws until limit or null byte
void draw_line(Surface *surface, int x, int y, uint32_t color, const char *str, uint32_t limit);
void fill_surface(Surface *surface, uint32_t color);

#include "interfaces/gpu.h"
#include "interfaces/blk.h"
#include "interfaces/input.h"

// src/kernel.c
typedef struct {
  GpuDev *gpu;
  BlkDev *blk_devices;
  InputDev *input_devices;
  uint32_t blk_devices_count;
  uint32_t input_devices_count;
  
} Hardware;
void kernel_init(Hardware *hw);
void kernel_update(Hardware *hw);

#endif

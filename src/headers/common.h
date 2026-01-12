#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

#include "input_codes.h"

typedef _Bool bool;
typedef unsigned char uint8_t;
typedef char int8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;

typedef struct {
  const char *ptr;
  uint32_t len;
} Str;

// Used for marking out parameters
#define out

#define NULL 0
#define true 1
#define false 0
#define PAGE_SIZE 4096

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

#define DEBUGD(var) \
  printf(STRINGIFY(var) "=%d\n", var)
#define DEBUGS(var) \
  printf(STRINGIFY(var) "='%s'\n", var)
#define DEBUGX(var) \
  printf(STRINGIFY(var) "=0x%x\n", var)

#define LOG(fmt, ...) \
  printf("[LOG] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, ##__VA_ARGS__)

#define ERROR(fmt, ...) \
  printf("[ERROR] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, ##__VA_ARGS__)

#define PANIC(fmt, ...) do { \
  printf("[PANIC] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, ##__VA_ARGS__); \
  for (;;) __asm__ __volatile__("wfi"); \
} while (0)

#define ASSERT(expr) do { \
  if (!(expr)) { \
    printf("[ASSERT] %s " __FILE__ ":" STRINGIFY(__LINE__) ": " STRINGIFY(expr) "\n", __func__); \
    for (;;) __asm__ __volatile__("wfi"); \
  } \
} while (0)

void putchar(char ch);
void printf(const char *fmt, ...);
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
uint32_t __bswapsi2(uint32_t u);

// src/uart.c
void uart_init(void);

// src/sbi.c
// NOTE: assembly source in boot.s
// It would be more efficient to use inline assembly,
// but those functions won't be called a lot, so it doesn't
// matter. Just a note for the future.

extern long sbi_set_timer(uint64_t stime_value);
extern long sbi_console_putchar(char ch);
extern long sbi_console_getchar(void);
extern long sbi_shutdown(void);

// src/plic.c
void plic_enable(uint32_t id);
void plic_set_priority(uint32_t id, uint8_t priority);
void plic_set_threshold(uint8_t threshold);
uint32_t plic_claim(void);
void plic_complete(uint32_t id);
void plic_enablep(uint32_t id, uint8_t priority);

// src/memory.c
typedef size_t paddr_t;
typedef size_t vaddr_t;

#define SATP_SV32 (1u << 31)

typedef enum {
  PAGE_V = (1 << 0),
  PAGE_R = (1 << 1),
  PAGE_W = (1 << 2),
  PAGE_X = (1 << 3),
  PAGE_U = (1 << 4),
  PAGE_G = (1 << 5),
  PAGE_A = (1 << 6),
  PAGE_D = (1 << 7),

  // 2 bits for os use
  // 22 bits page number
} PageEntryFlags;

paddr_t alloc_pages(uint32_t count);
void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags);

volatile uint8_t * const UART = (void *)0x10000000;
const uint32_t UART_INT = 10;

const uint32_t VIRTIO_MMIO_START = 0x10001000;
const uint32_t VIRTIO_INTERRUPT_START = 0;
const uint32_t VIRTIO_COUNT = 7;

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

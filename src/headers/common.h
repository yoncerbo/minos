#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

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

#define true 1
#define false 0
#define PAGE_SIZE 4096
#define SECTOR_SIZE 512

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
#define LIMIT_UP(a, b) MIN(a, b)
#define LIMIT_DOWN(a, b) MAX(a, b)

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

#endif

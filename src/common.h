#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON

#define true 1
#define false 0
#define bool _Bool

typedef unsigned char uint8_t;
typedef char int8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;

typedef size_t paddr_t;
typedef size_t vaddr_t;

typedef struct {
  const char *ptr;
  uint32_t len;
} Str;

#define CSTR_LEN(str) (sizeof(str) - 1)
#define STR(str) ((Str){ str, CSTR_LEN(str) })

#define PAGE_SIZE 4096

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x) 

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

#define DEBUGD(var) \
  printf(STRINGIFY(var) "=%d\n", var)

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LIMIT_UP(a, b) MIN(a, b)
#define LIMIT_DOWN(a, b) MAX(a, b)

volatile char * const UART = (char *)0x10000000;

const uint32_t VIRTIO_MMIO_START = 0x10001000;

void memcpy(void *restrict dest, const void *restrict src, size_t n) {
  char *d = dest, *s = src;
  for (uint32_t i = 0; i < n; ++i) d[i] = s[i];
}

// for the forseeable future, I'm just gonna use
// enums, as I don't need the flexibility of function pointers
typedef enum {
  FS_NONE,
  FS_FAT32,
} FsType;

typedef struct {
  FsType type;
} Fs;

typedef enum {
  ENTRY_NONE,
  ENTRY_FILE,
  ENTRY_DIR,
} EntryType;

typedef struct {
  uint32_t size;
  uint32_t start;
  EntryType type;
} DirEntry;

#endif

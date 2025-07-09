#ifndef INCLUDE_COMMON
#define INCLUDE_COMMON


typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef uint32_t size_t;

#define PAGE_SIZE 4096
typedef struct { void *inner } paddr_t;
typedef struct { void *inner } vaddr_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

volatile char *UART = (char *)0x10000000;

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

#endif

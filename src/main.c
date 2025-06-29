typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef uint32_t size_t;

typedef struct { void *inner } paddr_t;
typedef struct { void *inner } vaddr_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[];

volatile char *UART = (char *)0x10000000;

void putchar(char ch) {
  *UART = ch;
}

#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg

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
      case 'c':
        char ch = va_arg(vargs, int);
        putchar(ch);
        break;
      case 's':
        const char *s = va_arg(vargs, const char *);
        while (*s) putchar(*s++);
        break;
      case 'x':
        uint32_t v = va_arg(vargs, uint32_t);
        for (int i = 7; i >= 0; i--) {
          uint32_t nibble = (v >> (i * 4)) & 0xf;
          putchar("0123456789abcdef"[nibble]);
        }
        break;
      case 'd':
        int32_t value = va_arg(vargs, int);
        uint32_t  magnitude = value;
        if (value < 0) {
          putchar('-');
          magnitude = -magnitude;
        }

        uint32_t divisor = 1;
        while (magnitude / divisor > 9) divisor *= 10;
        while (divisor > 0) {
          putchar('0' + magnitude / divisor);
          magnitude %= divisor;
          divisor /= 10;
        }
    }
    fmt++;
  }
while_end:

  va_end(vargs);
}

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

#define PAGE_SIZE 4096

paddr_t alloc_pages(uint32_t count) {
  static paddr_t next_paddr = { HEAP_START };
  paddr_t paddr = next_paddr;
  next_paddr.inner += count * PAGE_SIZE;

  if (next_paddr.inner > HEAP_END) {
    PANIC("Failed to allocate %d pages: out of memory!\n", count);
  }

  // TODO: zero the memory
  return paddr;
}

void kernel_main(void) {
  LOG("Starting kernel...\n");
  alloc_pages(64);
  for (;;) __asm__ __volatile__("wfi");
}

__attribute__((aligned(4)))
__attribute__((interrupt("supervisor")))
void handle_interrupt(void) {
  uint32_t scause, stval, sepc;
  __asm__ __volatile__(
      "csrr %0, scause \n"
      "csrr %1, stval \n"
      "csrr %2, sepc \n"
      : "=r"(scause), "=r"(stval), "=r"(sepc)
  );
  PANIC("Interrupt: scause=%d, stval=%d, sepc=%x\n", scause, stval, sepc);
}

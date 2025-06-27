typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef uint32_t size_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];

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
  printf("[LOG] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, __VA_ARGS__)
#define ERROR(fmt, ...) \
  printf("[ERROR] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, __VA_ARGS__)

#define PANIC(fmt, ...) do { \
  printf("[PANIC] %s " __FILE__ ":" STRINGIFY(__LINE__) " " fmt, __func__, __VA_ARGS__); \
  for (;;) __asm__ __volatile__("wfi"); \
} while (0)


void kernel_main(void) {
  LOG("Starting kernel...\n", 0);
  PANIC("The end ", 0);
  for (;;) __asm__ __volatile__("wfi");
}


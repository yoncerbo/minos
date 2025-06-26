
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];

volatile char *UART = (char *)0x10000000;

void kernel_main(void) {
  *UART = 'H';
  *UART = 'e';
  *UART = 'l';
  *UART = 'l';
  *UART = 'o';
  *UART = '!';
  *UART = 10;
  for (;;) {
    __asm__ __volatile__("wfi");
  }
}



typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];

void kernel_main(void) {
  for (;;) {
    __asm__ __volatile__("wfi");
  }
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
  __asm__ __volatile__(
      "mv sp, %[STACK_TOP]\n"
      "j kernel_main\n"
      :
      : [STACK_TOP] "r" (STACK_TOP)
  );
}

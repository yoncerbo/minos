typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef uint32_t size_t;

typedef struct { void *inner } paddr_t;
typedef struct { void *inner } vaddr_t;

extern char BSS_START[], BSS_END[], STACK_TOP[];
extern char HEAP_START[], HEAP_END[], KERNEL_BASE[];

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
#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0)
#define PAGE_R (1 << 1)
#define PAGE_W (1 << 2)
#define PAGE_X (1 << 3)
#define PAGE_U (1 << 4)

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

// Page table entry
// 32-10 Page number
// 9-8 For use
// 7 Dirty
// 6 Accessed
// 5 Gobal
// 4 User
// 3 Executable
// 2 Writeable
// 1 Readable
// 0 Valid

// Virtual address
// 31-22 Level 1 index
// 21-12 Level 0 index
// 11-0 Offset

void map_page(uint32_t *table1, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
  uint32_t va = (uint32_t)vaddr.inner;
  uint32_t pa = (uint32_t)paddr.inner;
  if (va & 0x7) PANIC("unaligned vaddr %x\n", vaddr);
  if (pa & 0x7) PANIC("unaligned paddr %x\n", paddr);

  uint32_t vpn1 = (va >> 22) & 0x3ff;
  if ((table1[vpn1] & PAGE_V) == 0) {
    uint32_t pt_paddr = (uint32_t)alloc_pages(1).inner;
    table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
  }

  uint32_t vpn0 = (va >> 12) & 0x3ff;
  uint32_t *table0 = (uint32_t *)((table1[vpn1] >> 10) * PAGE_SIZE);
  table0[vpn0] = ((pa / PAGE_SIZE) << 10) | flags | PAGE_V;
}

void kernel_main(void) {
  LOG("Starting kernel...\n");

  uint32_t flags, *page_table = alloc_pages(1).inner;
  LOG("Mapping physical pages into virtual addresses, table at %x\n", page_table);

  flags = PAGE_R | PAGE_W | PAGE_X;
  LOG("|> kernel pages from=0x%x, to=0x%x\n", KERNEL_BASE, HEAP_END);
  for (char *paddr = KERNEL_BASE; paddr < HEAP_END; paddr += PAGE_SIZE) {
    map_page(page_table, (vaddr_t){ paddr }, (paddr_t){ paddr }, flags);
  }

  flags = PAGE_R | PAGE_W;
  uint32_t MMIO_START = 0x10000000; // UART
  uint32_t MMIO_END = 0x10000007; // UART_END
  LOG("|> mmio pages from=0x%x, to=0x%x\n", MMIO_START, MMIO_END); 
  for (char *paddr = (char *)MMIO_START; paddr < (char *)MMIO_END; paddr += PAGE_SIZE) {
    map_page(page_table, (vaddr_t){ paddr }, (paddr_t){ paddr }, flags);
  }

  LOG("Setting up virtual memory\n");
  __asm__ __volatile__(
      "sfence.vma\n"
      "csrw satp, %[satp]\n"
      "sfence.vma\n"
      :
      : [satp] "r" (SATP_SV32 | ((uint32_t)page_table / PAGE_SIZE))
  );

  LOG("Initialization finished\n");
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

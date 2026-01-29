#ifndef INCLUDE_CMN_LIB
#define INCLUDE_CMN_LIB

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

#define CASSERT(predicate) char assertion_failed##__LINE__[(predicate) ? 1 : -1]

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
  typedef int64_t isize_t;
#elif defined ARCH_RV32
  typedef uint32_t size_t;
  typedef int32_t isize_t;
#else
#error "Unknown architecture"
#endif

typedef _Bool bool;

typedef struct {
  uint32_t len;
  const char *ptr;
} Str;

#define NULL 0
#define true 1
#define false 0

#define SYSV __attribute__((sysv_abi))
#define ASM __asm__ __volatile__
#define INCLUDE_ASM(file) __asm__(".include \"" file "\"")
#define TRAP __builtin_trap
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define NORETURN __attribute__((noreturn))
#define UNREACHABLE __builtin_unreachable
#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg
#define bswap16(n) __builtin_bswap16(n)
#define bswap32(n) __builtin_bswap32(n)

#define STRINGIFY_INNER(x) #x
#define STRINGIFY(x) STRINGIFY_INNER(x) 
#define CSTR_LEN(str) (sizeof(str) - 1)
#define STR(str) ((Str){ CSTR_LEN(str), (str) })

#define SIZEOF_IN_PAGES(type) (sizeof(type) + PAGE_SIZE - 1) / PAGE_SIZE

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define UPPER_BOUND(a, b) MIN(a, b)
#define LOWER_BOUND(a, b) MAX(a, b)

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
bool are_strings_equal(const char *s1, const char *s2, size_t limit);
const char *strip_string(const char *str, uint32_t limit, uint32_t *out_len);
uint32_t __bswapsi2(uint32_t u);

#define DEBUGD(var) \
  log(STRINGIFY(var) "=%d", var)
#define DEBUGS(var) \
  log(STRINGIFY(var) "='%s'", var)
#define DEBUGX(var) \
  log(STRINGIFY(var) "=0x%x", var)

#define ASSERT(expr) do { \
  if (!(expr)) { \
    log("[ASSERT] %s " __FILE__ ":" STRINGIFY(__LINE__) ": " STRINGIFY(expr) "\n", __func__); \
    TRAP(); \
  } \
} while (0)

typedef enum {
  SYS_LOG = 1,
  SYS_EXIT = 2,
} SyscallType;

typedef enum {
  SYS_OK = 0,
  SYS_ERR_UNKNOWN_SYSCALL = 1,
  SYS_ERR_BAD_ARG = 2,
} SyscallError;

SyscallError sys_log(const char *str, size_t limit);
NORETURN void sys_exit(size_t error_code);

typedef enum {
  OK = 0,
  ERR_OUT_OF_SPACE,
} Error;

typedef struct Sink {
  Error (SYSV *write)(void *this, const void *buffer, uint32_t limit);
} Sink;

typedef struct {
  uint8_t *ptr;
  uint32_t capacity;
  uint32_t position;
} Buffer;

Error write(Sink *sink, const void *buffer, uint32_t limit);
Error write_str(Sink *sink, Str str);
Error writeb(Buffer *buffer, Sink *sink, const void *content, uint32_t limit);
Error write_strb(Buffer *buffer, Sink *sink, Str str);
void putcharb(Buffer *buffer, Sink *sink, char ch);
void vprintb(Buffer *buffer, Sink *sink, const char *format, va_list vargs);
void flush_buffer(Buffer *buffer, Sink *sink);
void prints(Sink *sink, const char *format, ...);
void log(const char *format, ...);

Sink *LOG_SINK = NULL;

#include "strings.c"
#include "print.c"

#endif

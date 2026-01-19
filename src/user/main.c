
#define ASM __asm__

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef uint64_t size_t;

typedef enum {
  SYS_LOG = 1,
  SYS_EXIT = 2,
} SyscallType;

typedef enum {
  SYS_OK = 0,
  SYS_ERR_UNKNOWN_SYSCALL = 1,
  SYS_ERR_BAD_ARG = 2,
} SyscallError;

SyscallError sys_log(const char *str, size_t limit) {
  size_t result = SYS_LOG;
  ASM("syscall" : "+a"(result) : "D"(str), "S"(limit));
  return result;
}

void sys_exit(size_t error_code) {
  size_t result = SYS_EXIT;
  ASM("syscall" :: "a"(result), "D"(error_code));
}

__attribute__((naked, section(".text.start")))
void _start(void) {

  sys_log("Hello from C binary", 19);

  sys_exit(12345);
}

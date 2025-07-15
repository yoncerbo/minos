#include "common.h"

// assembly source in boot.s
// It would be more efficient to use inline assembly,
// but those functions won't be called a lot, so it doesn't
// matter. Just a note for the future.

extern long sbi_set_timer(uint64_t stime_value);
extern long sbi_console_putchar(char ch);
extern long sbi_console_getchar(void);
extern long sbi_shutdown(void);

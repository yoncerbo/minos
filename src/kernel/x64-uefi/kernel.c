#include "cmn/lib.h"
#include "common.h"
#include "arch.h"

SYSV Error qemu_debugcon_write(void *data, const void *buffer, uint32_t limit) {
  Sink *this = data;
  const uint8_t *bytes = buffer;
  for (size_t i = 0; i < limit; ++i) {
    WRITE_PORT(0xE9, bytes[i]);
  }
  return OK;
}

void _start(BootData *data) {
  qemu_debugcon_write(0, "hello\n", 6);
  ASM("int3");

  for(;;) WFI();
}

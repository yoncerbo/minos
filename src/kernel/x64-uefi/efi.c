#include "common.h"
#include "arch.h"
#include "efi.h"

EFIAPI size_t efi_main(void *image_handle, EfiSystemTable *st) {
  EFI_SINK.out = st->con_out;
  LOG_SINK = &EFI_SINK.sink;
  st->con_out->clear_screen(st->con_out);

  size_t status = 0;

  // NOTE: The current stack in in the UEFI boot data memory,
  // we need to set up a new stack
  uint8_t *stack_top = &KERNEL_STACK[sizeof(KERNEL_STACK) - 1];
  ASM("mov rsp, %0 \n jmp kernel_main" :: "g"(stack_top));
  UNREACHABLE();
}


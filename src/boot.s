
.extern kernel_main
.extern STACK_TOP
.extern handle_interrupt

.section .text.boot

boot:
  la sp, STACK_TOP

  la t0, handle_interrupt
  csrw stvec, t0

  j kernel_main

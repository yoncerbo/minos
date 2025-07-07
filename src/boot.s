
.extern kernel_main
.extern STACK_TOP
.extern handle_interrupt

.section .text.boot

boot:
  # load stack pointer
  la sp, STACK_TOP

  # setup interrupt handler
  la t0, handle_interrupt
  csrw stvec, t0

  # enable external + software + timer interrupts
  li t0, (1 << 9) + (1 << 5) + (1 << 1)
  csrw sie, t0
  csrs sstatus, (1 << 1)

  # for testing software interrupt
  # csrs sip, 0b10

  # for testing exceptions
  # unimp

  # start executing kernel code
  j kernel_main

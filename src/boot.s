
.extern kernel_main
.extern STACK_TOP
.extern handle_supervisor_interrupt
.extern handle_machine_interrupt

.section .text.boot

boot:
  # load stack pointer
  la sp, STACK_TOP

  # setup interrupt handler
  la t0, handle_supervisor_interrupt
  csrw stvec, t0

  # enable external + software + timer interrupts
  # https://five-embeddev.github.io/riscv-docs-html/riscv-priv-isa-manual/latest-adoc/supervisor.html
  li t0, (1 << 9) + (1 << 5) + (1 << 1)
  csrw sie, t0
  csrs sstatus, (1 << 1)

  # Machine mode
  # la t0, handle_machine_interrupt
  # csrw mtvec, t0
  # li t0, (1 << 11) + (1 << 7) + (1 << 3)
  # csrw mie, t0
  # li t0, (0b11 << 11)
  # csrs mstatus, t0

  # la t0, kernel_main
  # csrw mepc, t0
  # mret

  # for testing software interrupt
  # csrs sip, 0b10

  # for testing exceptions
  # unimp

  # start executing kernel code
  j kernel_main

# c definition in src/sbi.c
# according to legacy spec at https://www.scs.stanford.edu/~zyedidia/docs/riscv/riscv-sbi.pdf

.global sbi_set_timer
sbi_set_timer:
  li a7, 0
  ecall
  ret

.global sbi_console_putchar
sbi_console_putchar:
  li a7, 1
  ecall
  ret

.global sbi_console_getchar
sbi_console_getchar:
  li a7, 2
  ecall
  ret

.global sbi_shutdown
sbi_shutdown:
  li a7, 8
  ecall
  ret

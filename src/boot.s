
.extern kernel_main
.extern STACK_TOP

.section .text.boot

boot:
  la sp, STACK_TOP
  j kernel_main

ENTRY(boot)

SECTIONS {
  . = 0x80200000;
  KERNEL_BASE = .;
  
  .text : {
    KEEP(*(.text.boot));
    *(.text .text.*)
  }

  .rodata : ALIGN(4) {
    *(.rodata .rodata.*);
  }

  .data : ALIGN(4) {
    *(.data .data.*);
    *(.sdata .sdata.*);
  }

  .bss : ALIGN(4) {
    BSS_START = .;
    *(.bss .bss.*);
    *(.sbss .sbss.*);
    BSS_END = .;
  }

  . = ALIGN(4);
  . += 128 * 1024;
  STACK_TOP = .;

  . = ALIGN(4096);
  HEAP_START = .;
  . += 128 * 1024 * 1024;
  HEAP_END = .;

}


.section .text

# inputs:
# rdi: pointer to gdi_table_ptr
.global load_gdt_table
load_gdt_table:
  lgdt [rdi]

  push 1 * 8 # push code segment offset in GDT_TABLE to stack
  lea rax, [rip + .reload_code_segment]
  push rax
  retfq # do a far return - like normal return, but after that pop value from
        # the stack and load it into code segment register
.reload_code_segment:

  mov ax, 2 * 8 # load data segment offset in GDT_TABLE
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  mov ax, 5 * 8 # TSS segment offset in GDT_TABLE
  ltr ax

  ret

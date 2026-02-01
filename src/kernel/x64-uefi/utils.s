.intel_syntax noprefix

.section .text

.equ GDT_KERNEL_CODE, 1
.equ GDT_KERNEL_DATA, 2
.equ GDT_USER_NULL, 3
.equ GDT_TSS_LOW, 6

# SOURCE: https://sandpile.org/x86/msr.htm

.equ MSR_EFER, 0xC0000080
.equ MSR_STAR, 0xC0000081
.equ MSR_LSTAR, 0xC0000082
.equ MSR_FMASK, 0xC0000084

.equ MSR_FS_BAS, 0xC0000100
.equ MSR_GS_BAS, 0xC0000101
.equ MSR_KERNEL_GS_BAS, 0xC0000102

.equ EFER_SCE, 1

.equ CTX_KERNEL_SP, 0
.equ CTX_USER_SP, 8

# inputs:
# rdi: pointer to gdi_table_ptr
.global load_gdt_table
load_gdt_table:
  lgdt [rdi]

  push GDT_KERNEL_CODE * 8
  lea rax, [rip + .reload_code_segment]
  push rax
  retfq # do a far return - like normal return, but after that pop value from
        # the stack and load it into code segment register
.reload_code_segment:

  mov ax, GDT_KERNEL_DATA * 8
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax

  mov ax, GDT_TSS_LOW * 8
  ltr ax

  ret

.global enable_system_calls
# input:
#   rdi: kernel gs base pointer
enable_system_calls:
  # SOURCE: https://sandpile.org/x86/msr.htm
  # NOTE: wrms - msr in ecx, low in eax, high in edx

  mov ecx, MSR_EFER
  rdmsr
  or eax, EFER_SCE # system call enable
  wrmsr

  mov ecx, MSR_STAR
  # CS - Code Selector, SS - Stack Selector
  # 3, 4, 1, 2
  rdmsr // keep the lower 32 bits in eax
  mov edx, 0x001B0008
  wrmsr

  mov ecx, MSR_LSTAR
  # lea rax, syscall_stub
  .extern syscall_entry
  lea rax, syscall_entry
  mov rdx, rax
  shr rdx, 32
  wrmsr

  mov ecx, MSR_FMASK
  mov eax, 0xFFFFFFFD
  mov edx, 0xFFFFFFFF
  wrmsr

  mov ecx, MSR_KERNEL_GS_BAS
  mov eax, edi
  shr rdi, 32
  mov edx, edi
  wrmsr

  mov ecx, MSR_FS_BAS
  xor eax, eax
  xor edx, edx
  wrmsr
  mov ecx, MSR_GS_BAS
  wrmsr

  swapgs # we're in kernel, so we're gonna use
         # kernel gs base
  ret

# Cpu pushes those on the stack during an interrupt
# ss, rsp, rflags, cs, rip

.extern interrupt_handler

interrupt_stub:
  # NOTE: We're skipping rsp
  push r15
  push r14
  push r13
  push r12
  push r11
  push r10
  push r9
  push r8
  push rbp
  push rdi
  push rsi
  push rdx
  push rcx
  push rbx
  push rax

  mov rdi, rsp
  call interrupt_handler
  mov rsp, rax

  pop rax
  pop rbx
  pop rcx
  pop rdx
  pop rsi
  pop rdi
  pop rbp
  pop r8
  pop r9
  pop r10
  pop r11
  pop r12
  pop r13
  pop r14
  pop r15

  add rsp, 16 # vector number and error code
  iretq

.set i, 0

.macro isr_error count
  .rept \count
    .align 16
    push i
    jmp interrupt_stub
    .set i, i + 1
  .endr
.endm

.macro isr_no_error count
  .rept \count
    .align 16
    push 0
    push i
    jmp interrupt_stub
    .set i, i + 1
  .endr
.endm

.align 16
.global ISR_VECTORS
ISR_VECTORS:
isr_no_error 8
isr_error 1
isr_no_error 1
isr_error 5
isr_no_error 2
isr_error 1
isr_no_error 3
isr_error  1
isr_no_error 2
isr_error 2
isr_no_error (256 - 26)

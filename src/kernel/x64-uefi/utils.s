.intel_syntax noprefix

.section .data

.global PSF_FONT_START
PSF_FONT_START:
# .incbin "res/font.psf"
  .incbin "res/font1.psf"

.align 4096
.global USER_BINARY
USER_BINARY:
  .incbin "out/x64-uefi/user_main.bin"
.align 4096
.global USER_BINARY_END
USER_BINARY_END:
int3 # A safeguard

.section .text

.equ GDT_KERNEL_CODE, 1
.equ GDT_KERNEL_DATA, 2
.equ GDT_TSS_LOW, 5

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

.extern handle_syscall

USER_SP: .quad 0
syscall_stub:
  swapgs # swap KERNEL_GS_BAS and GS_BAS MSRs
  mov gs:CTX_USER_SP, rsp
  mov rsp, gs:CTX_KERNEL_SP

  push r11
  push r10
  push r9
  push r8
  push rcx
  push rdx
  push rsi
  push rdi
  push rax

  # system v calling convention
  # preserved by the function: rbx, rsp, rbp, r12, r13, r14, r15
  # scrathch registers: rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
  mov rdi, rsp
  call handle_syscall
  test rax, rax
  jnz .user_program_exit

  pop rax
  pop rdi
  pop rsi
  pop rdx
  pop rcx
  pop r8
  pop r9
  pop r10
  pop r11

  mov rsp, gs:CTX_USER_SP
  swapgs
  sysretq

.user_program_exit:
  pop rax # return code is in the rax
  add rsp, 8 * 8
  # NOTE: The rsp in set for kernel stack.
  # We entered use program from a function call.
  # The return address is on the kernel stack.
  # We can just return.
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
  # User CS, User SS, Kernel CS, Kernel SS, 32 bits offset
  # 4, 5, 1, 2
  mov edx, 0x00130008
  wrmsr

  mov ecx, MSR_LSTAR
  lea rax, syscall_stub
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

.global run_user_program
# inputs:
#   rdi: user function pointer
#   rsi: user stack pointer
run_user_program:
  mov gs:CTX_KERNEL_SP, rsp
  mov rcx, rdi # new instruction pointer
  mov rsp, rsi # new stack pointer
  mov r11, 0x0202 # eflags

  swapgs # swap into user gs base
  # SOURCE: https://www.felixcloutier.com/x86/sysret
  sysretq

.global putchar_qemu_debugcon
# inputs:
#   rdi: character to output
putchar_qemu_debugcon:
  mov rax, rdi
  out 0xE9, al
  ret

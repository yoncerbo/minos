default rel
bits 64

SYS_LOG: equ 1
SYS_EXIT: equ 2

mov rax, SYS_LOG
lea rdi, [msg]
mov rsi, msg_len
syscall

mov rax, SYS_EXIT
mov rdi, 1234
syscall

msg: db "Hello, Userspace!"
msg_len: equ $ - msg

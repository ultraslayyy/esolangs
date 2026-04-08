BITS 64
org 0x400000

ehdr:
    db 0x7f,"ELF",2,1,1,0
    times 8 db 0
    dw 2
    dw 0x3e
    dd 1
    dq _start
    dq phdr - $$
    dq 0
    dd 0
    dw 64
    dw 56
    dw 1
    dw 0
    dw 0
    dw 0

phdr:
    dd 1
    dd 5
    dq 0
    dq $$
    dq $$
    dq filesize
    dq filesize
    dq 0x1000

_start:
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel msg]
    mov rdx, 14
    syscall

    mov rax, 60
    xor rdi, rdi
    syscall

msg:
    db "Hello, world!", 10

filesize equ $ - $$
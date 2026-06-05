section .text
    ; global _start  ; Commented out - using standard C runtime
    global syscall3
    ; extern main  ; Commented out - not needed with standard C runtime

; _start:  ; Commented out - conflicts with Scrt1.o
;     call main
;     mov rdi, 60       ; syscall number for exit
;     xor rsi, rsi      ; status 0
;     xor rdx, rdx
;     xor rcx, rcx
;     call syscall3

syscall3:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    syscall
    ret

global syscall6

syscall6:
    mov rax, rdi      ; syscall number
    mov rdi, rsi      ; arg1
    mov rsi, rdx      ; arg2
    mov rdx, rcx      ; arg3
    mov r10, r8       ; arg4
    mov r8, r9        ; arg5
    mov r9, [rsp+8]   ; arg6 (from stack)
    syscall
    ret


global syscall
syscall:
    jmp syscall6


section .note.GNU-stack noalloc noexec nowrite progbits

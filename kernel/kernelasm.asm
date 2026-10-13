[BITS 64]
DEFAULT ABS
section .text.entry
global kernel_start
extern main
extern common_isr
kernel_start:
  mov rsp,0xFFFFFFFF80020000 
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov rax, vga_address
mov [rax + 10],'P'
mov [rax + 11],0x0A

call main
jmp $

global idt_load

idt_load:
    lidt [rdi]
    ret

global aiueo
aiueo:
  sti
  ret

%macro ISR 1
global isr%1

isr%1:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, %1
    jmp dummy_isr
%endmacro

%assign i 0
%rep 256
    ISR i
    %assign i i + 1
%endrep

global aisr13
aisr13:
    cli

    ; CPUが自動で積んだもの:
    ; [rsp + 0]  = error code
    ; [rsp + 8]  = RIP
    ; [rsp + 16] = CS
    ; [rsp + 24] = RFLAGS
    ;
    ; CPL変更時はさらに
    ; [rsp + 32] = RSP
    ; [rsp + 40] = SS
    mov rdi, 13
    call common_isr
    
    jmp $
    add rsp, 8          ; #GPのerror codeを捨てる
    sti
    iretq
extern common_isr14
global aisr14
aisr14:
    cli

    ; CPUが自動で積んだもの:
    ; [rsp + 0]  = error code
    ; [rsp + 8]  = RIP
    ; [rsp + 16] = CS
    ; [rsp + 24] = RFLAGS
    ;
    ; CPL変更時はさらに
    ; [rsp + 32] = RSP
    ; [rsp + 40] = SS
    mov rax, [rsp]      ; error code
    mov rbx, [rsp + 8]  ; RIP
    mov rcx, cr2        ; CR2

    mov rdi, rax        ; 第1引数 = error code
    mov rsi, rbx        ; 第2引数 = RIP
    mov rdx, rcx        ; 第3引数 = CR2

    call common_isr14
    
    jmp $
    add rsp, 8          ; #GPのerror codeを捨てる
    sti
    iretq    

extern aaaa
dummy_isr:
  call common_isr
  pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

  iretq

global abcd
abcd:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
   call aaaa
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
   iretq

section .text
vga_address equ 0xFFFF8000000B8000
section .rodata
global isr_table
isr_table:
%assign i 0
%rep 256
    dq isr%+i
    %assign i i + 1
%endrep
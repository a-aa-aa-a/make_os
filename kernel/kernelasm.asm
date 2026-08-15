[BITS 64]
DEFAULT ABS
section .text.entry
global kernel_start
extern main
kernel_start:
  cli

  mov rsp,0xFFFFFFFF80001000 
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov rax, vga_address
mov [rax + 10],'P'
mov [rax + 11],0x0A

call main
jmp $


section .text
vga_address equ 0xFFFF8000000B8000
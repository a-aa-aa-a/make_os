[BITS 32]
section .text.entry
global to_longmode
extern main
to_longmode:

  mov [0xB8006],'T'
  mov [0xB8007],0x0C
  call main

  lgdt [gdt_descripter]
  
  mov eax, cr4 
  or eax, (1 << 5) ; PAE 
  mov cr4, eax

  call setup_paging

  mov eax, PML4 
  mov cr3, eax

  mov ecx, 0xC0000080 ; IA32_EFER 
  rdmsr 
  or eax, (1 << 8) ; LME 
  or eax, 1 << 11
  wrmsr

  
  mov eax, cr0
  or  eax, 0x80000000     ; bit31 = PG
  mov cr0, eax

  jmp 0x08:pm_entry

setup_paging:
  push edi
  push ecx
  push eax

  xor eax, eax

  mov edi, PML4
  xor eax, eax
  mov ecx, (7 * 4096) / 4
  rep stosd

  mov dword [PML4 + 511*8],     PDPT | 0x03
  mov dword [PDPT + 510*8],     PD | 0x03
  mov dword [PD + 0*8],     PT | 0x03

  mov ebx,0xB003
  mov edi,PT
  mov ecx, 90
  .fill_pd:
      mov dword [edi], ebx
      add ebx, 0x1000     ; 次のエントリは2MB先の物理アドレスを指す
      add edi, 8              ; ページテーブルの1エントリは8バイト
      loop .fill_pd

  
  ; まず拡張CPUIDの最大番号を確認
oo:
  mov dword [PML4 + 256*8],     PDPT3 | 0x03
  mov eax, 0x80000000
  cpuid
  
  cmp eax, 0x80000001
  jb .no_1gib

  ; 1GiB page対応を確認
  mov eax, 0x80000001
  cpuid

  test edx, (1 << 26)
  jz .no_1gib
  ; -----------------
  ; 1GiB page 対応
  ; -----------------
  .yes_1gib:

    ; ここで1GiBページを使う
      mov dword [PDPT3 + 0*8],      0x00000083
      jmp .done
  .no_1gib:
    ; 1GiBページ非対応
    ; 2MiBページを使う
    mov dword [PDPT3 + 0*8],      PD3 |0x03
    
    mov ebx,0x83
    mov edi,PD3
    mov ecx, 512
  .fill_pd2:
      mov dword [edi], ebx
      add ebx, 0x200000     ; 次のエントリは2MB先の物理アドレスを指す
      add edi, 8              ; ページテーブルの1エントリは8バイト
      loop .fill_pd2
      jmp .done

.done:
  mov dword [PML4 + 0*8],     PDPT | 0x03
  mov dword [PDPT + 0*8],     PD2 | 0x03
  mov dword [PD2 + 0*8],     PT2 | 0x03
  mov dword [PT2 + 0xA*8],     0xA003


  pop eax
  pop ecx
  pop edi
  ret
;FFFF80000000
[BITS 64]
pm_entry:
  mov rsp,0xFFFFFFFF00010000 
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov ss, ax
  
  mov rax,0xFFFFFFFF80000000

  jmp rax

[BITS 32]  
section .text
align 8
gdt_start:
    dq 0                            ; 0x00: 未使用(GDTの先頭は必ず空にする決まり)
    dq 0x00AF9A000000FFFF ;0x08
    dq 0x00AF92000000FFFF ;0x10
gdt_end:

gdt_descripter:
  dw gdt_end - gdt_start - 1
  dd gdt_start

PML4 equ 0x1000
PDPT equ 0x2000
PD   equ 0x3000
PT   equ 0x4000
PD2 equ 0x5000
PT2 equ 0x6000
PDPT3 equ 0x7000
PD3 equ 0x8000

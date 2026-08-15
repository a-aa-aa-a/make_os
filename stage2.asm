[ORG 0xA600]
[BITS 16]
start:
    
  mov ax,0xB800
  mov ds,ax
  mov [ds:2],'2'
  mov [ds:3],0x0F
  xor ax,ax
  mov ds,ax
  call GetE820
  cli
  
  call enable_a20
  lgdt [gdt_descripter]
  
  mov eax,cr0
  or eax,0x1
  mov cr0,eax
  
  jmp CODE_SEL:protected_mode  
  hang:
    jmp $
E820Buffer equ 0x9b00
E820Count equ 0x9a00
enable_a20:
  in al,0x92
  or al,0x02
  out 0x92,al
  call check_a20
  cmp al,1
  je .a20_done
  .fail:
    mov ax,0xB800
    mov ds,ax
    mov [ds:2],'E'
    mov [ds:3],0x0A
    jmp $
  .a20_done:
  mov ax,0xB800
  mov ds,ax
  mov [ds:2],'S'
  mov [ds:3],0x0B
  xor ax,ax
  mov ds,ax
  ret
check_a20:

  pushf
  push ds
  push es
  push di
  push si

  cli 

  xor ax,ax
  mov ds,ax
  mov di,0x500

  mov ax,0xFFFF
  mov es,ax
  mov si,0x510

  mov al,[ds:di]
  push ax
  mov al,[es:si]
  push ax
  

  mov byte [ds:di],0x00
  mov byte [es:si],0xFF

  pop ax
  mov [es:si],al
  pop ax
  mov [ds:di],al


  cmp byte [ds:di],0xFF
  
  mov ax,0
  je .check
  mov ax,1

  .check:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret

GetE820:
    xor ebx, ebx               ; continuation = 0
    mov di, E820Buffer         ; ES:DI = バッファ
    mov ax,cs
    mov es,ax                 ; 同じセグメントに書く
    .next_entry:
      mov eax, 0xE820            ; E820 呼び出し
      mov edx, 0x534D4150            ; 文字列 'SMAP'
      mov ecx, 24                ; バッファサイズ（24バイト）
      int 0x15                   ; BIOS 呼び出し

      jc .done                   ; CF=1 → 終了
      cmp eax, 0x534D4150             ; 正しい返却か？
      jne .done

    ; エントリを保存
      add di, 24                 ; 次のバッファ位置へ移動
      inc word [E820Count]       ; エントリ数++

      test ebx, ebx              ; EBX=0 → 終了
      jnz .next_entry
      .done:
        ret
[BITS 32]
protected_mode:
  mov ax,DATA_SEL
  mov ds,ax
  mov es,ax
  mov ss,ax
  mov fs,ax
  mov gs,ax
  mov esp,0x90000

  mov [0xB8004],'P'
  mov [0xB8005],0x0E
  mov al, [gdt_descripter]
  mov bx, [gdt_descripter + 2]
  
  jmp 0xA800

[BITS 16]



align 8
gdt_start:
    dq 0                            ; 0x00: 未使用(GDTの先頭は必ず空にする決まり)
    dq 0x00CF9A000000FFFF ;0x08
    dq 0x00CF92000000FFFF ;0x10
gdt_end:

gdt_descripter:
  dw gdt_end - gdt_start - 1
  dd gdt_start
CODE_SEL     equ 0x08
DATA_SEL     equ 0x10

times 512 - ($ - $$) db 0
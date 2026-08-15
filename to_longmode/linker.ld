ENTRY(to_longmode)

SECTIONS
{
    . = 0x0000A800;

    .text : {
        *(.text.entry)   /* kernel_entry を含むセクションを必ず先頭に置く */
                /* 残りの通常のコードはその後ろに続く */
        *(.rodata*)
        *(.data*)
        *(COMMON)
        *(.bss*)
    }
}


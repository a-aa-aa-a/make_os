ENTRY(kernel_start)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text : {
        *(.text.entry)   /* kernel_entry を含むセクションを必ず先頭に置く */
                /* 残りの通常のコードはその後ろに続く */
        *(.rodata*)
        *(.data*)
        *(COMMON)
        *(.bss*)
    }
}


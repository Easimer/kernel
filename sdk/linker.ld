OUTPUT_FORMAT(binary)

SECTIONS {
    . = 0x00500;

    .exeh : {
        *(.exeh);
    }

    .text : {
        *(.text);
    }

    .data : {
        *(.data);
        *(.bss);
        *(.rodata);
    }
}

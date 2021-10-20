SECTIONS {
    output : {
        *(.text.entrypoint)
        *(.text*)
        *(.rodata*)
    }

    got : {
        *(.got*)
    }

    /DISCARD/ : {
        *(*);
    }
}

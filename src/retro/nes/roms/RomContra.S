    .section .rodata
    .global rom_contra_data
    .global rom_contra_size
    .align 4

rom_contra_data:
    .incbin "rom/076. Contra.nes"
rom_contra_end:

rom_contra_size:
    .word rom_contra_end - rom_contra_data


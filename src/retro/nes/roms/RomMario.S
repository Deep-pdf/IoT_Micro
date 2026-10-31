    .section .rodata
    .global rom_mario_data
    .global rom_mario_size
    .align 4

rom_mario_data:
    .incbin "rom/Super Mario Bros. 3 (USA) (Rev 1).nes"
rom_mario_end:

rom_mario_size:
    .word rom_mario_end - rom_mario_data


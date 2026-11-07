    .section .rodata
    .global rom_radracer_data
    .global rom_radracer_size
    .align 4

rom_radracer_data:
    .incbin "rom/Rad Racer II (USA).nes"
rom_radracer_end:

rom_radracer_size:
    .word rom_radracer_end - rom_radracer_data


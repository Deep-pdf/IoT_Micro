    .section .rodata
    .global gb_rom_bank0_data
    .global gb_rom_bank0_size
    .global gb_rom_compressed_data
    .global gb_rom_compressed_size
    .align 4

gb_rom_bank0_data:
    .incbin "rom/pokemon_red_bank0.bin"
gb_rom_bank0_end:

gb_rom_bank0_size:
    .word gb_rom_bank0_end - gb_rom_bank0_data

    .align 4
gb_rom_compressed_data:
    .incbin "rom/pokemon_red_banks1_63.lz4"
gb_rom_compressed_end:

gb_rom_compressed_size:
    .word gb_rom_compressed_end - gb_rom_compressed_data


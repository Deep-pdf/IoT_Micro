import zlib

data = open('scratch/spiffs_partition.bin', 'rb').read()
print(f"Partition Dump Size: {len(data)} bytes")

ff_count = data.count(b'\xff')
print(f"0xFF bytes: {ff_count} / {len(data)} ({ff_count * 100.0 / len(data):.2f}%)")

names = [b'pokemon_red.sav', b'pokemon_red.sav.tmp', b'pokemon_red.sav.bak', b'littlefs', b'LittleFS', b'runtime_config']
for name in names:
    pos = data.find(name)
    if pos != -1:
        print(f"Found '{name.decode()}' at offset 0x{pos:X}")
    else:
        print(f"'{name.decode()}' NOT FOUND")

# LittleFS superblock check
# In LittleFS v2, blocks typically start with metadata pairs
print("First 128 bytes hex dump:")
for row in range(0, 128, 16):
    hex_str = ' '.join(f'{b:02X}' for b in data[row:row+16])
    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[row:row+16])
    print(f"0x{row:04X}: {hex_str} | {ascii_str}")

# Check any non-0xFF blocks
non_ff_blocks = []
block_size = 4096
for b_idx in range(len(data) // block_size):
    blk = data[b_idx*block_size : (b_idx+1)*block_size]
    if blk != b'\xff' * block_size:
        non_ff_blocks.append((b_idx, b_idx * block_size))

print(f"Total non-0xFF 4KB blocks: {len(non_ff_blocks)} / {len(data) // block_size}")
for b_idx, offset in non_ff_blocks[:10]:
    print(f"  Block {b_idx} at offset 0x{offset:X}")


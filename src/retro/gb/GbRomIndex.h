#ifndef GB_ROM_INDEX_H
#define GB_ROM_INDEX_H

#include <stdint.h>

#define GB_ROM_TOTAL_BANKS          64
#define GB_ROM_BANK_SIZE            16384
#define GB_ROM_BANK0_SIZE           16384
#define GB_ROM_COMPRESSED_DATA_SIZE 439472

struct GbRomBankMeta {
    uint8_t  bankNum;       // Bank number (0..63)
    uint32_t flashOffset;   // Offset in compressed data (or 0 for bank 0)
    uint16_t compSize;      // Compressed length in bytes
    uint16_t uncompSize;    // Expected decompressed length (16384)
    uint32_t crc32;         // CRC32 of uncompressed bank
};

extern const GbRomBankMeta GB_ROM_BANKS[GB_ROM_TOTAL_BANKS];

#endif // GB_ROM_INDEX_H

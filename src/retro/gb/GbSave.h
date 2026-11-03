#ifndef GB_SAVE_H
#define GB_SAVE_H

#include <stdint.h>
#include <stddef.h>

#define GB_SAVE_RAM_BANKS      4
#define GB_SAVE_RAM_BANK_SIZE  8192
#define GB_SAVE_RAM_TOTAL_SIZE (GB_SAVE_RAM_BANKS * GB_SAVE_RAM_BANK_SIZE) // 32 KB

class GbSave {
public:
    // Future integration interface for 32 KB MBC3 cartridge SRAM
    virtual ~GbSave() = default;

    // Direct byte access to 8 KB cartridge RAM banks ($A000 - $BFFF)
    virtual uint8_t readRam(uint8_t bank, uint16_t offset) = 0;
    virtual void    writeRam(uint8_t bank, uint16_t offset, uint8_t value) = 0;

    // Direct pointer to currently active 8 KB RAM bank (for fast emulator access)
    virtual uint8_t* getRamBankPtr(uint8_t bank) = 0;

    // Persistence lifecycle
    virtual bool load() = 0;
    virtual bool save() = 0;
    virtual bool isDirty() const = 0;
    virtual void clearDirty() = 0;

    // Metadata & integrity verification
    virtual uint32_t calculateSaveCrc32() = 0;
    virtual bool     verifySaveIntegrity() = 0;
};

#endif // GB_SAVE_H


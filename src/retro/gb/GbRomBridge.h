#ifndef GB_ROM_BRIDGE_H
#define GB_ROM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Cartridge metadata extracted from Bank 0 header
typedef struct {
    char     title[17];        // Null-terminated title (0x0134 - 0x0143)
    uint8_t  cartridge_type;   // Cartridge type byte (0x0147, e.g. 0x13 for MBC3+RAM+BATTERY)
    uint8_t  rom_size_code;    // ROM size code (0x0148, 0x05 for 1MB / 64 banks)
    uint8_t  ram_size_code;    // RAM size code (0x0149, 0x03 for 32KB / 4 banks)
    uint16_t global_checksum;  // Global checksum (0x014E - 0x014F)
    uint8_t  header_checksum;  // Header checksum (0x014D)
    bool     cgb_flag;         // True if CGB flag (0x0143) is set
} gb_rom_metadata_t;

// Lifecycle
bool gb_rom_bridge_init(void);
bool gb_rom_bridge_is_initialized(void);

// Bank 0 direct pointer & bounded reads
const uint8_t* gb_rom_bridge_get_bank0(void);
uint8_t        gb_rom_bridge_read_bank0(uint16_t address);

// Switchable bank bounded reads & bank selection
uint8_t        gb_rom_bridge_read_switchable(uint16_t address);
bool           gb_rom_bridge_select_bank(uint8_t bank);
uint8_t        gb_rom_bridge_get_current_bank(void);

// Cartridge validation & metadata
bool gb_rom_bridge_get_metadata(gb_rom_metadata_t *out_metadata);
bool gb_rom_bridge_validate_cartridge(void);

// Telemetry & metrics
uint32_t gb_rom_bridge_get_decompression_count(void);
uint32_t gb_rom_bridge_get_cache_hit_count(void);
void     gb_rom_bridge_reset_metrics(void);

// Diagnostic Heap Logging
void gb_diag_log_heap(const char *checkpoint_tag, int bank_idx, size_t req_size, void *ptr, const char *allocator_desc);

#ifdef __cplusplus
}
#endif

#endif // GB_ROM_BRIDGE_H


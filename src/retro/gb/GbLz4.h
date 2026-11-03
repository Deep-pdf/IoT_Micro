#ifndef GB_LZ4_H
#define GB_LZ4_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Safe in-place LZ4 raw block decompressor.
 * Designed for microcontrollers (0 working RAM, stack/registers only).
 * 
 * @param source Pointer to compressed LZ4 raw block
 * @param dest Pointer to destination output buffer
 * @param compressedSize Byte length of compressed data
 * @param maxDecompressedSize Maximum capacity of dest buffer (e.g. 16384)
 * @return int Number of decompressed bytes on success, negative error code on failure
 */
int gb_lz4_decompress_safe(const uint8_t* source, uint8_t* dest, int compressedSize, int maxDecompressedSize);

#endif // GB_LZ4_H


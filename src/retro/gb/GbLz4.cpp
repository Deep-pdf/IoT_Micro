#include "GbLz4.h"
#include <string.h>

int gb_lz4_decompress_safe(const uint8_t* source, uint8_t* dest, int compressedSize, int maxDecompressedSize) {
    if (!source || !dest || compressedSize <= 0 || maxDecompressedSize <= 0) {
        return -1;
    }

    const uint8_t* srcPtr = source;
    const uint8_t* srcEnd = source + compressedSize;
    uint8_t* dstPtr = dest;
    uint8_t* dstEnd = dest + maxDecompressedSize;

    while (srcPtr < srcEnd) {
        uint8_t token = *srcPtr++;

        // 1. Literal length decoding
        size_t litLen = (token >> 4);
        if (litLen == 15) {
            uint8_t s;
            do {
                if (srcPtr >= srcEnd) return -2;
                s = *srcPtr++;
                litLen += s;
            } while (s == 255);
        }

        // 2. Copy literals
        if (srcPtr + litLen > srcEnd || dstPtr + litLen > dstEnd) {
            return -3; // Overflow error
        }
        memcpy(dstPtr, srcPtr, litLen);
        dstPtr += litLen;
        srcPtr += litLen;

        // Check if this was the last sequence of the block
        if (srcPtr >= srcEnd) {
            break;
        }

        // 3. Match offset (16-bit little-endian)
        if (srcPtr + 2 > srcEnd) {
            return -4;
        }
        uint16_t offset = (uint16_t)srcPtr[0] | ((uint16_t)srcPtr[1] << 8);
        srcPtr += 2;
        if (offset == 0) {
            return -5; // Invalid offset 0
        }

        // 4. Match length decoding
        size_t matchLen = (token & 0x0F) + 4;
        if ((token & 0x0F) == 15) {
            uint8_t s;
            do {
                if (srcPtr >= srcEnd) return -6;
                s = *srcPtr++;
                matchLen += s;
            } while (s == 255);
        }

        // 5. Match copying with overlap support
        if ((dstPtr - offset) < dest || (dstPtr + matchLen) > dstEnd) {
            return -7; // Invalid match reference or buffer overrun
        }

        const uint8_t* matchSrc = dstPtr - offset;
        while (matchLen--) {
            *dstPtr++ = *matchSrc++;
        }
    }

    return (int)(dstPtr - dest);
}


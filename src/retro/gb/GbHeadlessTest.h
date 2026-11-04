#ifndef GB_HEADLESS_TEST_H
#define GB_HEADLESS_TEST_H

#include <stdint.h>
#include <stdbool.h>

class GbHeadlessTest {
public:
    // Runs headless emulation test for specified number of frames (default: 120 frames = 2 seconds at 60 FPS)
    // Logs comprehensive telemetry to Serial:
    // - Memory usage before / after init / after run / after deinit
    // - ROM header and MBC3 validation
    // - Frame progression, CPU PC, SP, active ROM and RAM banks
    // - Bank decompression counts and cache hits
    static bool run(uint32_t targetFrames = 120);
};

#endif // GB_HEADLESS_TEST_H


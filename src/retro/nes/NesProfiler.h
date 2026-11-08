#ifndef NES_PROFILER_H
#define NES_PROFILER_H

#include <Arduino.h>

// Diagnostic compile-time flag (can be set to 0 in production release)
#ifndef NES_PERF_DIAGNOSTICS
#define NES_PERF_DIAGNOSTICS 1
#endif

#if NES_PERF_DIAGNOSTICS

struct NesPerfMetrics {
    // Current frame microsecond accumulators
    uint32_t frameCpuUs;
    uint32_t framePpuBgUs;
    uint32_t framePpuSpriteUs;
    uint32_t frameMapperUs;
    uint32_t frameMmc3IrqCount;
    uint32_t frameBankWriteCount;
    uint32_t frameRenderedLines;
    uint32_t frameSkippedLines;

    // Window accumulators
    uint32_t windowFrameUs;
    uint32_t minFrameUs;
    uint32_t maxFrameUs;

    uint32_t windowCpuUs;
    uint32_t windowPpuBgUs;
    uint32_t windowPpuSpriteUs;
    uint32_t windowMapperUs;
    uint32_t windowRendUs;
    uint32_t windowTftUs;

    uint32_t windowMmc3Irqs;
    uint32_t windowBankWrites;
    uint32_t windowRenderedLines;
    uint32_t windowSkippedLines;

    uint32_t frameCount;
};

extern NesPerfMetrics g_nesPerf;

inline void NesPerf_init() {
    memset(&g_nesPerf, 0, sizeof(g_nesPerf));
    g_nesPerf.minFrameUs = 0xFFFFFFFF;
}

inline void NesPerf_startFrame() {
    g_nesPerf.frameCpuUs = 0;
    g_nesPerf.framePpuBgUs = 0;
    g_nesPerf.framePpuSpriteUs = 0;
    g_nesPerf.frameMapperUs = 0;
    g_nesPerf.frameMmc3IrqCount = 0;
    g_nesPerf.frameBankWriteCount = 0;
    g_nesPerf.frameRenderedLines = 0;
    g_nesPerf.frameSkippedLines = 0;
}

inline void NesPerf_endFrame(uint32_t frameTimeUs, uint32_t rendTimeUs, uint32_t tftTimeUs) {
    g_nesPerf.frameCount++;
    g_nesPerf.windowFrameUs += frameTimeUs;
    if (frameTimeUs < g_nesPerf.minFrameUs) g_nesPerf.minFrameUs = frameTimeUs;
    if (frameTimeUs > g_nesPerf.maxFrameUs) g_nesPerf.maxFrameUs = frameTimeUs;

    g_nesPerf.windowCpuUs += g_nesPerf.frameCpuUs;
    g_nesPerf.windowPpuBgUs += g_nesPerf.framePpuBgUs;
    g_nesPerf.windowPpuSpriteUs += g_nesPerf.framePpuSpriteUs;
    g_nesPerf.windowMapperUs += g_nesPerf.frameMapperUs;
    g_nesPerf.windowRendUs += rendTimeUs;
    g_nesPerf.windowTftUs += tftTimeUs;

    g_nesPerf.windowMmc3Irqs += g_nesPerf.frameMmc3IrqCount;
    g_nesPerf.windowBankWrites += g_nesPerf.frameBankWriteCount;
    g_nesPerf.windowRenderedLines += g_nesPerf.frameRenderedLines;
    g_nesPerf.windowSkippedLines += g_nesPerf.frameSkippedLines;
}

inline void NesPerf_resetWindow() {
    g_nesPerf.frameCount = 0;
    g_nesPerf.windowFrameUs = 0;
    g_nesPerf.minFrameUs = 0xFFFFFFFF;
    g_nesPerf.maxFrameUs = 0;
    g_nesPerf.windowCpuUs = 0;
    g_nesPerf.windowPpuBgUs = 0;
    g_nesPerf.windowPpuSpriteUs = 0;
    g_nesPerf.windowMapperUs = 0;
    g_nesPerf.windowRendUs = 0;
    g_nesPerf.windowTftUs = 0;
    g_nesPerf.windowMmc3Irqs = 0;
    g_nesPerf.windowBankWrites = 0;
    g_nesPerf.windowRenderedLines = 0;
    g_nesPerf.windowSkippedLines = 0;
}

#endif // NES_PERF_DIAGNOSTICS
#endif // NES_PROFILER_H


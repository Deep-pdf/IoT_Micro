#ifndef APU_STUB_H
#define APU_STUB_H

#include <stdint.h>

class Cpu6502;
class Bus;

class Apu2A03 {
public:
    void connectCPU(Cpu6502*) {}
    void connectBus(Bus*) {}
    void reset() {}
    void cpuWrite(uint16_t, uint8_t) {}
    void clock() {}
    void setVolume(uint8_t) {}
};

#endif // APU_STUB_H

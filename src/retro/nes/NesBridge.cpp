#include "NesBridge.h"
#include "NesDisplayAdapter.h"
#include "core/cpu6502.h"
#include "core/cartridge.h"

static Cpu6502* nesCpu = nullptr;
static Cartridge* activeCart = nullptr;
static bool emulatorRunning = false;

bool NesBridge::init(Adafruit_ST7735* display) {
    NesDisplayAdapter::init(display);

    if (!nesCpu) {
        nesCpu = new Cpu6502();
    }

    nesCpu->bus.ppu.setScanlineIndexedCallback(NesDisplayAdapter::renderScanlineIndexed);
    emulatorRunning = false;
    return true;
}

bool NesBridge::loadRom(const uint8_t* romData, size_t romSize) {
    if (!nesCpu || !romData || romSize == 0) return false;

    if (activeCart) {
        delete activeCart;
        activeCart = nullptr;
    }

    activeCart = new Cartridge(romData, romSize);
    if (!activeCart->isValid()) {
        delete activeCart;
        activeCart = nullptr;
        emulatorRunning = false;
        return false;
    }

    nesCpu->bus.insertCartridge(activeCart);
    nesCpu->reset();

    NesDisplayAdapter::clearBorders();
    emulatorRunning = true;
    return true;
}

bool NesBridge::loadRom(const NesRom& rom) {
    return loadRom(rom.data, rom.size);
}

bool NesBridge::loadGame(NesGameId game) {
    const NesRom* rom = NesGetRom(game);
    if (!rom) return false;
    return loadRom(*rom);
}

void NesBridge::setController(uint8_t state) {
    if (nesCpu) {
        nesCpu->bus.setController(state);
    }
}

uint8_t NesBridge::getController() {
    if (nesCpu) {
        return nesCpu->bus.getControllerState();
    }
    return 0x00;
}

void NesBridge::runFrame() {
    if (!emulatorRunning || !nesCpu || !activeCart) return;

    NesDisplayAdapter::startFrame();
    nesCpu->clockFrame();
    NesDisplayAdapter::endFrame();
}

void NesBridge::reset() {
    if (nesCpu && activeCart) {
        nesCpu->reset();
    }
}

void NesBridge::stop() {
    emulatorRunning = false;
    if (activeCart) {
        delete activeCart;
        activeCart = nullptr;
    }
}

bool NesBridge::isRunning() {
    return emulatorRunning && (activeCart != nullptr);
}


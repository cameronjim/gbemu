#include "bus.hpp"

namespace gb {

namespace {
constexpr uint16_t kRegDma = 0xFF46;
} // namespace

void Bus::tick_access() {
    // pandocs: every sm83 memory access occupies one m-cycle; components see mid-instruction time
    timer_.tick(4);
    ppu_.tick(4);
    apu_.tick(4);
    access_cycles_ += 4;
}

uint8_t Bus::read8(uint16_t addr) {
    tick_access();
    return peek8(addr);
}

uint8_t Bus::peek8(uint16_t addr) {
    if (addr <= 0x7FFF) {
        return mapper_ != nullptr ? mapper_->read_rom(addr) : 0xFF;
    }
    if (addr <= 0x9FFF) {
        return ppu_.read_vram(static_cast<uint16_t>(addr - 0x8000));
    }
    if (addr <= 0xBFFF) {
        return mapper_ != nullptr ? mapper_->read_ram(static_cast<uint16_t>(addr - 0xA000)) : 0xFF;
    }
    if (addr <= 0xFDFF) {
        // echo ram mirrors wram, switched bank included
        return wram_at(addr);
    }
    if (addr <= 0xFE9F) {
        return ppu_.read_oam(static_cast<uint16_t>(addr - 0xFE00));
    }
    if (addr <= 0xFEFF) {
        // unusable region
        return 0xFF;
    }
    if (addr <= 0xFF7F) {
        return read_io(addr);
    }
    if (addr <= 0xFFFE) {
        return hram_[addr - 0xFF80];
    }
    return ie_;
}

void Bus::write8(uint16_t addr, uint8_t value) {
    tick_access();
    if (addr <= 0x7FFF) {
        if (mapper_ != nullptr) {
            mapper_->write_rom(addr, value);
        }
        return;
    }
    if (addr <= 0x9FFF) {
        ppu_.write_vram(static_cast<uint16_t>(addr - 0x8000), value);
        return;
    }
    if (addr <= 0xBFFF) {
        if (mapper_ != nullptr) {
            mapper_->write_ram(static_cast<uint16_t>(addr - 0xA000), value);
        }
        return;
    }
    if (addr <= 0xFDFF) {
        wram_at(addr) = value;
        return;
    }
    if (addr <= 0xFE9F) {
        ppu_.write_oam(static_cast<uint16_t>(addr - 0xFE00), value);
        return;
    }
    if (addr <= 0xFEFF) {
        return;
    }
    if (addr <= 0xFF7F) {
        write_io(addr, value);
        return;
    }
    if (addr <= 0xFFFE) {
        hram_[addr - 0xFF80] = value;
        return;
    }
    ie_ = value;
}

uint16_t Bus::read16(uint16_t addr) {
    const uint8_t lo = read8(addr);
    const uint8_t hi = read8(static_cast<uint16_t>(addr + 1));
    return static_cast<uint16_t>((hi << 8) | lo);
}

void Bus::write16(uint16_t addr, uint16_t value) {
    write8(addr, static_cast<uint8_t>(value & 0xFF));
    write8(static_cast<uint16_t>(addr + 1), static_cast<uint8_t>(value >> 8));
}

uint8_t Bus::read_io(uint16_t addr) {
    if (addr >= kRegNr10 && addr <= kWaveRamEnd) {
        return apu_.read_register(addr);
    }
    switch (addr) {
    case kRegJoyp:
        return joypad_.read();
    case kRegSb:
        return serial_.read_sb();
    case kRegSc:
        return serial_.read_sc();
    case kRegDiv:
        return timer_.read_div();
    case kRegTima:
        return timer_.read_tima();
    case kRegTma:
        return timer_.read_tma();
    case kRegTac:
        return timer_.read_tac();
    case kRegLcdc:
    case kRegStat:
    case kRegScy:
    case kRegScx:
    case kRegLy:
    case kRegLyc:
    case kRegBgp:
    case kRegObp0:
    case kRegObp1:
    case kRegWy:
    case kRegWx:
    case kRegVbk:
        return ppu_.read_register(addr);
    case kRegIf:
        // upper 3 bits read as 1
        return static_cast<uint8_t>(0xE0 | irq_.read());
    case kRegDma:
        return dma_;
    case kRegKey1:
        // read-only stub: bit 7 single speed, bit 0 no switch armed, bits 1-6 read 1
        return cgb_ ? 0x7E : 0xFF;
    case kRegSvbk:
        // pandocs: svbk unused bits read 1
        return cgb_ ? static_cast<uint8_t>(0xF8 | svbk_) : 0xFF;
    default:
        // unmapped io reads 0xFF, never 0x00
        return 0xFF;
    }
}

void Bus::write_io(uint16_t addr, uint8_t value) {
    if (addr >= kRegNr10 && addr <= kWaveRamEnd) {
        apu_.write_register(addr, value);
        return;
    }
    switch (addr) {
    case kRegJoyp:
        joypad_.write(value);
        break;
    case kRegSb:
        serial_.write_sb(value);
        break;
    case kRegSc:
        serial_.write_sc(value);
        break;
    case kRegDiv:
        timer_.write_div();
        break;
    case kRegTima:
        timer_.write_tima(value);
        break;
    case kRegTma:
        timer_.write_tma(value);
        break;
    case kRegTac:
        timer_.write_tac(value);
        break;
    case kRegLcdc:
    case kRegStat:
    case kRegScy:
    case kRegScx:
    case kRegLy:
    case kRegLyc:
    case kRegBgp:
    case kRegObp0:
    case kRegObp1:
    case kRegWy:
    case kRegWx:
    case kRegVbk:
        ppu_.write_register(addr, value);
        break;
    case kRegSvbk:
        if (cgb_) {
            svbk_ = static_cast<uint8_t>(value & 0x07);
        }
        break;
    case kRegIf:
        irq_.write(value);
        break;
    case kRegDma: {
        dma_ = value;
        // v1 decision: instant copy of 160 bytes from xx00; no time passes
        const uint16_t src = static_cast<uint16_t>(value << 8);
        for (uint8_t i = 0; i < 0xA0; ++i) {
            ppu_.write_oam(i, peek8(static_cast<uint16_t>(src + i)));
        }
        break;
    }
    default:
        break;
    }
}

} // namespace gb

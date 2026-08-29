#include "bus.hpp"

namespace gb {

namespace {
constexpr uint16_t kRegDma = 0xFF46;
} // namespace

uint32_t Bus::video_cycles(uint32_t cpu_cycles) {
    if (!double_speed_) {
        return cpu_cycles;
    }
    // pandocs "key1": double speed doubles the cpu clock only, the ppu and apu keep their rate
    const uint32_t total = cpu_cycles + video_carry_;
    video_carry_ = static_cast<uint8_t>(total & 1u);
    return total >> 1;
}

void Bus::tick_components(uint32_t cpu_cycles) {
    timer_.tick(cpu_cycles);
    const uint32_t dots = video_cycles(cpu_cycles);
    ppu_.tick(dots);
    apu_.tick(dots);
    run_hdma_chunks(ppu_.take_hblank_entries());
}

void Bus::tick_access() {
    // pandocs: every sm83 memory access occupies one m-cycle; components see mid-instruction time
    tick_components(4);
    access_cycles_ += 4;
}

bool Bus::commit_speed_switch() {
    if (!cgb_ || !speed_armed_) {
        return false;
    }
    double_speed_ = !double_speed_;
    speed_armed_ = false;
    video_carry_ = 0;
    // pandocs "key1": the switch resets the div counter
    timer_.write_div();
    return true;
}

uint8_t Bus::read_hdma5() const {
    // pandocs: bit 7 set means no transfer is running, bits 0-6 hold the remaining length minus one
    const uint8_t left = static_cast<uint8_t>((hdma_remaining_ - 1) & 0x7F);
    return static_cast<uint8_t>((hdma_active_ ? 0x00 : 0x80) | left);
}

void Bus::write_hdma5(uint8_t value) {
    if ((value & 0x80) == 0 && hdma_active_) {
        // pandocs: clearing bit 7 mid-transfer stops it, leaving the remaining length readable
        hdma_active_ = false;
        return;
    }
    // pandocs: both addresses ignore their low 4 bits, and the destination is always in vram
    // v1: pandocs names rom and wram as the only source regions, but the range is not enforced
    hdma_src_ = static_cast<uint16_t>(hdma_latch_src_ & 0xFFF0);
    hdma_dst_ = static_cast<uint16_t>((hdma_latch_dst_ & 0x1FF0) | 0x8000);
    hdma_remaining_ = static_cast<uint8_t>((value & 0x7F) + 1);
    hdma_active_ = (value & 0x80) != 0;
    if (!hdma_active_) {
        // v1: general purpose dma copies the whole block instantly, no cpu stall modeled
        while (hdma_remaining_ > 0) {
            copy_hdma_chunk();
        }
    }
}

void Bus::copy_hdma_chunk() {
    for (uint8_t i = 0; i < 0x10; ++i) {
        // the source goes through the normal read path so rom and wram banking apply
        ppu_.write_vram(static_cast<uint16_t>(hdma_dst_ & 0x1FFF), peek8(hdma_src_));
        hdma_src_ = static_cast<uint16_t>(hdma_src_ + 1);
        hdma_dst_ = static_cast<uint16_t>(hdma_dst_ + 1);
    }
    --hdma_remaining_;
    if (hdma_remaining_ == 0) {
        hdma_active_ = false;
    }
}

void Bus::run_hdma_chunks(uint32_t hblanks) {
    for (uint32_t i = 0; i < hblanks && hdma_active_; ++i) {
        copy_hdma_chunk();
    }
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
    case kRegBcps:
    case kRegBcpd:
    case kRegOcps:
    case kRegOcpd:
    case kRegOpri:
        return ppu_.read_register(addr);
    case kRegIf:
        // upper 3 bits read as 1
        return static_cast<uint8_t>(0xE0 | irq_.read());
    case kRegDma:
        return dma_;
    case kRegKey1:
        // pandocs: bit 7 is the current speed, bit 0 the armed switch, bits 1-6 read 1
        return cgb_
                   ? static_cast<uint8_t>((double_speed_ ? 0x80 : 0x00) | 0x7E | (speed_armed_ ? 0x01 : 0x00))
                   : 0xFF;
    case kRegHdma1:
    case kRegHdma2:
    case kRegHdma3:
    case kRegHdma4:
        // pandocs: the hdma address registers are write-only
        return 0xFF;
    case kRegHdma5:
        return cgb_ ? read_hdma5() : 0xFF;
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
    case kRegBcps:
    case kRegBcpd:
    case kRegOcps:
    case kRegOcpd:
    case kRegOpri:
        ppu_.write_register(addr, value);
        break;
    case kRegSvbk:
        if (cgb_) {
            svbk_ = static_cast<uint8_t>(value & 0x07);
        }
        break;
    case kRegKey1:
        if (cgb_) {
            // only the arm bit is writable
            speed_armed_ = (value & 0x01) != 0;
        }
        break;
    case kRegHdma1:
        if (cgb_) {
            hdma_latch_src_ = static_cast<uint16_t>((hdma_latch_src_ & 0x00FF) | (value << 8));
        }
        break;
    case kRegHdma2:
        if (cgb_) {
            hdma_latch_src_ = static_cast<uint16_t>((hdma_latch_src_ & 0xFF00) | value);
        }
        break;
    case kRegHdma3:
        if (cgb_) {
            hdma_latch_dst_ = static_cast<uint16_t>((hdma_latch_dst_ & 0x00FF) | (value << 8));
        }
        break;
    case kRegHdma4:
        if (cgb_) {
            hdma_latch_dst_ = static_cast<uint16_t>((hdma_latch_dst_ & 0xFF00) | value);
        }
        break;
    case kRegHdma5:
        if (cgb_) {
            write_hdma5(value);
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

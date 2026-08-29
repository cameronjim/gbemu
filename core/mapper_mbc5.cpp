#include "mapper_mbc5.hpp"

#include <utility>

namespace gb {

MapperMbc5::MapperMbc5(std::vector<uint8_t> rom, uint32_t ram_size)
    : rom_(std::move(rom)), ram_(ram_size, 0) {}

uint8_t MapperMbc5::read_rom(uint16_t addr) const {
    if (rom_.empty()) {
        return 0xFF;
    }
    uint32_t bank = 0;
    if (addr >= 0x4000) {
        // pandocs: mbc5 has no bank-0 quirk, a written 0 maps bank 0 here
        bank = rom_bank_ & (rom_bank_count() - 1);
    }
    return rom_[bank * 0x4000 + (addr & 0x3FFF)];
}

void MapperMbc5::write_rom(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        ram_enable_ = (value & 0x0F) == 0x0A;
    } else if (addr < 0x3000) {
        rom_bank_ = static_cast<uint16_t>((rom_bank_ & 0x0100) | value);
    } else if (addr < 0x4000) {
        // pandocs: only bit 0 of this register is bank bit 8
        rom_bank_ = static_cast<uint16_t>((rom_bank_ & 0x00FF) | ((value & 0x01) << 8));
    } else if (addr < 0x6000) {
        ram_bank_ = static_cast<uint8_t>(value & 0x0F);
    }
    // 0x6000-0x7fff is unmapped: mbc5 has no latch or mode register
}

uint8_t MapperMbc5::read_ram(uint16_t addr) const {
    if (!ram_enable_ || ram_.empty()) {
        return 0xFF;
    }
    return ram_[(static_cast<uint32_t>(ram_bank_) * 0x2000 + addr) % ram_.size()];
}

void MapperMbc5::write_ram(uint16_t addr, uint8_t value) {
    if (!ram_enable_ || ram_.empty()) {
        return;
    }
    ram_[(static_cast<uint32_t>(ram_bank_) * 0x2000 + addr) % ram_.size()] = value;
}

} // namespace gb

#pragma once

#include "mapper.hpp"

#include <cstdint>
#include <vector>

namespace gb {

// rumble carts (0x1c-0x1e) map identically; the motor is not emulated
class MapperMbc5 final : public Mapper {
public:
    MapperMbc5(std::vector<uint8_t> rom, uint32_t ram_size);
    uint8_t read_rom(uint16_t addr) const override;
    void write_rom(uint16_t addr, uint8_t value) override;
    uint8_t read_ram(uint16_t addr) const override;
    void write_ram(uint16_t addr, uint8_t value) override;
    std::span<uint8_t> external_ram() override {
        return ram_;
    }
    size_t state_size() const override {
        return 4 + ram_.size();
    }
    void save_state(StateWriter& w) const override {
        w.b(ram_enable_);
        w.u16(rom_bank_);
        w.u8(ram_bank_);
        w.bytes(ram_);
    }
    void load_state(StateReader& r) override {
        ram_enable_ = r.b();
        rom_bank_ = static_cast<uint16_t>(r.u16() & 0x01FF);
        ram_bank_ = static_cast<uint8_t>(r.u8() & 0x0F);
        r.bytes(ram_);
    }

private:
    uint32_t rom_bank_count() const {
        return static_cast<uint32_t>(rom_.size() / 0x4000);
    }

    std::vector<uint8_t> rom_;
    std::vector<uint8_t> ram_;
    bool ram_enable_ = false;
    // 9 bits: low 8 written at 0x2000, bit 8 at 0x3000
    uint16_t rom_bank_ = 1;
    uint8_t ram_bank_ = 0;
};

} // namespace gb

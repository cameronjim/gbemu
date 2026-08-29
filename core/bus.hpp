#pragma once

#include "apu.hpp"
#include "interrupts.hpp"
#include "joypad.hpp"
#include "mapper.hpp"
#include "memory.hpp"
#include "ppu.hpp"
#include "serial.hpp"
#include "state.hpp"
#include "timer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace gb {

inline constexpr uint16_t kRegKey1 = 0xFF4D;
inline constexpr uint16_t kRegHdma1 = 0xFF51;
inline constexpr uint16_t kRegHdma2 = 0xFF52;
inline constexpr uint16_t kRegHdma3 = 0xFF53;
inline constexpr uint16_t kRegHdma4 = 0xFF54;
inline constexpr uint16_t kRegHdma5 = 0xFF55;
inline constexpr uint16_t kRegSvbk = 0xFF70;

inline constexpr size_t kWramBankSize = 0x1000;
inline constexpr size_t kWramBanks = 8;

class Bus final : public Memory {
public:
    Bus(Serial& serial, Timer& timer, Ppu& ppu, Apu& apu, Joypad& joypad, InterruptLine& irq)
        : serial_(serial), timer_(timer), ppu_(ppu), apu_(apu), joypad_(joypad), irq_(irq) {}

    void attach_mapper(Mapper& mapper) {
        mapper_ = &mapper;
    }

    // cgb only; dmg keeps the 0xD000 window on bank 1 and svbk/key1/hdma dead
    void set_cgb_mode(bool cgb) {
        cgb_ = cgb;
        svbk_ = 0;
        double_speed_ = false;
        speed_armed_ = false;
        video_carry_ = 0;
        hdma_active_ = false;
        hdma_remaining_ = 0;
    }

    uint8_t read8(uint16_t addr) override;
    void write8(uint16_t addr, uint8_t value) override;
    uint8_t peek8(uint16_t addr) override;
    uint16_t read16(uint16_t addr);
    void write16(uint16_t addr, uint16_t value);
    bool commit_speed_switch() override;
    // the one place cpu time is handed to every component; run_frame spends the remainder here
    void tick_components(uint32_t cpu_cycles);
    bool double_speed() const {
        return double_speed_;
    }
    // cycles already ticked into components by this instruction's accesses
    uint32_t take_access_cycles() {
        const uint32_t cycles = access_cycles_;
        access_cycles_ = 0;
        return cycles;
    }

    static constexpr size_t kStateSize = kWramBanks * kWramBankSize + 0x7F + 16;
    void save_state(StateWriter& w) const {
        for (const auto& bank : wram_) {
            w.bytes(bank);
        }
        w.bytes(hram_);
        w.u8(ie_);
        w.u8(dma_);
        w.u8(svbk_);
        w.b(double_speed_);
        w.b(speed_armed_);
        w.u8(video_carry_);
        w.u16(hdma_latch_src_);
        w.u16(hdma_latch_dst_);
        w.u16(hdma_src_);
        w.u16(hdma_dst_);
        w.u8(hdma_remaining_);
        w.b(hdma_active_);
    }
    void load_state(StateReader& r) {
        for (auto& bank : wram_) {
            r.bytes(bank);
        }
        r.bytes(hram_);
        ie_ = r.u8();
        dma_ = r.u8();
        const uint8_t svbk = static_cast<uint8_t>(r.u8() & 0x07);
        // dmg has no svbk; 0 is its only legal value
        svbk_ = cgb_ ? svbk : 0;
        const bool double_speed = r.b();
        const bool speed_armed = r.b();
        video_carry_ = static_cast<uint8_t>(r.u8() & 1);
        hdma_latch_src_ = r.u16();
        hdma_latch_dst_ = r.u16();
        hdma_src_ = r.u16();
        hdma_dst_ = r.u16();
        const uint8_t remaining = r.u8();
        // hdma5 can request at most 0x80 chunks
        hdma_remaining_ = remaining <= 0x80 ? remaining : 0x80;
        const bool hdma_active = r.b();
        // dmg has neither a speed switch nor hdma
        double_speed_ = cgb_ && double_speed;
        speed_armed_ = cgb_ && speed_armed;
        hdma_active_ = cgb_ && hdma_active && hdma_remaining_ > 0;
    }

private:
    uint8_t read_io(uint16_t addr);
    void write_io(uint16_t addr, uint8_t value);
    void tick_access();
    // ppu and apu always run at single-speed dots; the odd cycle carries so nothing is lost
    uint32_t video_cycles(uint32_t cpu_cycles);
    uint8_t read_hdma5() const;
    void write_hdma5(uint8_t value);
    void copy_hdma_chunk();
    void run_hdma_chunks(uint32_t hblanks);

    // pandocs: svbk 0 selects bank 1; dmg has a single fixed 0xD000 bank
    uint8_t wram_bank() const {
        return svbk_ == 0 ? 1 : svbk_;
    }
    uint8_t& wram_at(uint16_t addr) {
        const uint8_t bank = (addr & 0x1000) != 0 ? wram_bank() : 0;
        return wram_[bank][addr & 0x0FFF];
    }

    Serial& serial_;
    Timer& timer_;
    Ppu& ppu_;
    Apu& apu_;
    Joypad& joypad_;
    InterruptLine& irq_;
    // non-owning, attached at rom load
    Mapper* mapper_ = nullptr;
    std::array<std::array<uint8_t, kWramBankSize>, kWramBanks> wram_{};
    std::array<uint8_t, 0x7F> hram_{};
    uint8_t ie_ = 0x00;
    // pandocs power-up: dma reads 0xFF on dmg
    uint8_t dma_ = 0xFF;
    // raw 3-bit svbk write; 0 reads back as written but banks like 1
    uint8_t svbk_ = 0;
    bool cgb_ = false;
    uint32_t access_cycles_ = 0;
    bool double_speed_ = false;
    bool speed_armed_ = false;
    // the leftover half cpu-cycle owed to the ppu and apu
    uint8_t video_carry_ = 0;
    // hdma1/2 and hdma3/4 as written; pandocs makes all four write-only
    uint16_t hdma_latch_src_ = 0;
    uint16_t hdma_latch_dst_ = 0;
    // live pointers, latched from the write-only registers on each hdma5 start
    uint16_t hdma_src_ = 0;
    uint16_t hdma_dst_ = 0x8000;
    // chunks of 0x10 bytes still owed
    uint8_t hdma_remaining_ = 0;
    bool hdma_active_ = false;
};

} // namespace gb

#pragma once

#include "apu.hpp"
#include "bus.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "interrupts.hpp"
#include "joypad.hpp"
#include "ppu.hpp"
#include "serial.hpp"
#include "timer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace gb {

class Gameboy {
public:
    bool load_rom(std::span<const uint8_t> bytes);
    void run_frame();
    std::span<const uint8_t> framebuffer() const;
    void set_button(Button b, bool pressed);
    void set_serial_sink(Serial::Sink sink);
    void save_state(std::vector<uint8_t>& out) const;
    // hostile input: validated structurally before anything is applied
    bool load_state(std::span<const uint8_t> blob);
    // drains stereo interleaved s16 samples at ~48khz
    size_t read_audio(std::span<int16_t> out) {
        return apu_.read_audio(out);
    }
    uint64_t cycles() const {
        return cycles_;
    }
    // fixed at load_rom from the cart header; dmg carts run exactly as before
    bool cgb_mode() const {
        return cgb_mode_;
    }
    // per-pixel tile source for frontend colorization
    std::span<const uint16_t> framebuffer_tiles() const {
        return ppu_.tile_ids();
    }
    // debug accessor for the frontend tile viewer
    std::span<const uint8_t> debug_vram() const {
        return ppu_.vram();
    }
    // debug accessor for the mooneye fibonacci protocol
    const CpuRegs& debug_regs() const {
        return cpu_.regs();
    }
    bool has_battery() const {
        return cart_.has_value() && cart_->has_battery();
    }
    // battery save path; empty without a cart or external ram
    std::span<uint8_t> external_ram() {
        return cart_.has_value() ? cart_->mapper().external_ram() : std::span<uint8_t>{};
    }
    void set_rtc_seconds(uint64_t seconds) {
        if (cart_.has_value()) {
            cart_->mapper().set_rtc_seconds(seconds);
        }
    }

private:
    void render_test_pattern();

    Serial serial_;
    InterruptLine irq_;
    Timer timer_{irq_};
    Ppu ppu_{irq_};
    Apu apu_;
    Joypad joypad_;
    Bus bus_{serial_, timer_, ppu_, apu_, joypad_, irq_};
    Cpu cpu_{bus_};
    std::optional<Cartridge> cart_;
    bool cgb_mode_ = false;
    uint64_t cycles_ = 0;
    std::array<uint8_t, kLcdWidth * kLcdHeight> pattern_{};
};

} // namespace gb

#include "gameboy.hpp"

#include "state.hpp"

#include <utility>

namespace gb {

namespace {
constexpr std::array<uint8_t, 4> kStateMagic = {'G', 'B', 'S', 'T'};
// v2: banked vram/wram and the cgb bank registers; v1 blobs are rejected
constexpr uint32_t kStateVersion = 2;
} // namespace

namespace {
constexpr uint32_t kBandWidth = kLcdWidth / 4;
// dmg frame is 70224 t-cycles
constexpr uint32_t kFrameCycles = 70224;
} // namespace

bool Gameboy::load_rom(std::span<const uint8_t> bytes) {
    std::optional<Cartridge> cart = Cartridge::parse(bytes);
    if (!cart.has_value()) {
        return false;
    }
    cart_ = std::move(cart);
    cgb_mode_ = cart_->cgb();
    bus_.attach_mapper(cart_->mapper());
    bus_.set_cgb_mode(cgb_mode_);
    ppu_.set_cgb_mode(cgb_mode_);
    cpu_.set_boot_state(cgb_mode_);
    return true;
}

void Gameboy::run_frame() {
    if (cart_.has_value()) {
        // run to the vblank edge so the framebuffer never mixes two scanouts
        const uint64_t frame_start = ppu_.frame_count();
        uint32_t elapsed = 0;
        while (ppu_.frame_count() == frame_start) {
            // lcd off never reaches vblank; spend one frame of cycles instead
            if (elapsed >= kFrameCycles && !ppu_.lcd_enabled()) {
                break;
            }
            if (elapsed >= 2 * kFrameCycles) {
                break;
            }
            const uint32_t t = cpu_.step();
            if (t == 0) {
                // cpu trapped an unknown opcode and stopped
                break;
            }
            // the bus already ticked one m-cycle per access; spend the remainder here
            const uint32_t ticked = bus_.take_access_cycles();
            const uint32_t remainder = t > ticked ? t - ticked : 0;
            timer_.tick(remainder);
            ppu_.tick(remainder);
            apu_.tick(remainder);
            elapsed += t;
        }
        cycles_ += elapsed;
        return;
    }
    render_test_pattern();
}

std::span<const uint8_t> Gameboy::framebuffer() const {
    return cart_.has_value() ? ppu_.framebuffer() : std::span<const uint8_t>(pattern_);
}

void Gameboy::set_button(Button b, bool pressed) {
    joypad_.set_button(b, pressed);
}

void Gameboy::set_serial_sink(Serial::Sink sink) {
    serial_.set_sink(std::move(sink));
}

void Gameboy::save_state(std::vector<uint8_t>& out) const {
    out.clear();
    StateWriter w(out);
    w.bytes(kStateMagic);
    w.u32(kStateVersion);
    const auto section = [&out, &w](auto&& write_payload) {
        w.u32(0);
        const size_t start = out.size();
        write_payload();
        const uint32_t len = static_cast<uint32_t>(out.size() - start);
        out[start - 4] = static_cast<uint8_t>(len & 0xFF);
        out[start - 3] = static_cast<uint8_t>((len >> 8) & 0xFF);
        out[start - 2] = static_cast<uint8_t>((len >> 16) & 0xFF);
        out[start - 1] = static_cast<uint8_t>(len >> 24);
    };
    section([&] { cpu_.save_state(w); });
    section([&] { w.u8(irq_.read()); });
    section([&] { timer_.save_state(w); });
    section([&] { joypad_.save_state(w); });
    section([&] { serial_.save_state(w); });
    section([&] { bus_.save_state(w); });
    section([&] { ppu_.save_state(w); });
    section([&] { apu_.save_state(w); });
    section([&] {
        if (cart_.has_value()) {
            cart_->mapper().save_state(w);
        }
    });
}

bool Gameboy::load_state(std::span<const uint8_t> blob) {
    if (!cart_.has_value()) {
        return false;
    }
    // phase 1: structural validation, nothing is applied on any failure
    const std::array<size_t, 9> expected = {
        Cpu::kStateSize,
        1,
        Timer::kStateSize,
        Joypad::kStateSize,
        Serial::kStateSize,
        Bus::kStateSize,
        Ppu::kStateSize,
        Apu::kStateSize,
        cart_->mapper().state_size(),
    };
    if (blob.size() < 8) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (blob[i] != kStateMagic[i]) {
            return false;
        }
    }
    StateReader header(blob.subspan(4, 4));
    if (header.u32() != kStateVersion) {
        return false;
    }
    std::array<std::span<const uint8_t>, 9> sections;
    size_t pos = 8;
    for (size_t i = 0; i < sections.size(); ++i) {
        if (blob.size() - pos < 4) {
            return false;
        }
        StateReader len_reader(blob.subspan(pos, 4));
        const uint32_t len = len_reader.u32();
        pos += 4;
        if (len != expected[i] || blob.size() - pos < len) {
            return false;
        }
        sections[i] = blob.subspan(pos, len);
        pos += len;
    }
    if (pos != blob.size()) {
        return false;
    }
    // phase 2: apply; every component re-masks values to legal ranges
    StateReader r0(sections[0]);
    cpu_.load_state(r0);
    StateReader r1(sections[1]);
    irq_.write(r1.u8());
    StateReader r2(sections[2]);
    timer_.load_state(r2);
    StateReader r3(sections[3]);
    joypad_.load_state(r3);
    StateReader r4(sections[4]);
    serial_.load_state(r4);
    StateReader r5(sections[5]);
    bus_.load_state(r5);
    StateReader r6(sections[6]);
    ppu_.load_state(r6);
    StateReader r7(sections[7]);
    apu_.load_state(r7);
    StateReader r8(sections[8]);
    cart_->mapper().load_state(r8);
    return true;
}

void Gameboy::render_test_pattern() {
    // four-band pattern shown only when no cartridge is loaded
    for (uint32_t y = 0; y < kLcdHeight; ++y) {
        for (uint32_t x = 0; x < kLcdWidth; ++x) {
            pattern_[y * kLcdWidth + x] = static_cast<uint8_t>(x / kBandWidth);
        }
    }
}

} // namespace gb

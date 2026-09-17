#pragma once

#include "state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gb {

inline constexpr uint16_t kRegNr10 = 0xFF10;
inline constexpr uint16_t kRegNr11 = 0xFF11;
inline constexpr uint16_t kRegNr12 = 0xFF12;
inline constexpr uint16_t kRegNr13 = 0xFF13;
inline constexpr uint16_t kRegNr14 = 0xFF14;
inline constexpr uint16_t kRegNr21 = 0xFF16;
inline constexpr uint16_t kRegNr22 = 0xFF17;
inline constexpr uint16_t kRegNr23 = 0xFF18;
inline constexpr uint16_t kRegNr24 = 0xFF19;
inline constexpr uint16_t kRegNr30 = 0xFF1A;
inline constexpr uint16_t kRegNr31 = 0xFF1B;
inline constexpr uint16_t kRegNr32 = 0xFF1C;
inline constexpr uint16_t kRegNr33 = 0xFF1D;
inline constexpr uint16_t kRegNr34 = 0xFF1E;
inline constexpr uint16_t kRegNr41 = 0xFF20;
inline constexpr uint16_t kRegNr42 = 0xFF21;
inline constexpr uint16_t kRegNr43 = 0xFF22;
inline constexpr uint16_t kRegNr44 = 0xFF23;
inline constexpr uint16_t kRegNr50 = 0xFF24;
inline constexpr uint16_t kRegNr51 = 0xFF25;
inline constexpr uint16_t kRegNr52 = 0xFF26;
inline constexpr uint16_t kWaveRamStart = 0xFF30;
inline constexpr uint16_t kWaveRamEnd = 0xFF3F;

class Apu {
public:
    Apu() {
        // pandocs power-up: the only nonzero raw values behind the read masks
        regs_[kRegNr12 - kRegNr10] = 0xF3;
        regs_[kRegNr50 - kRegNr10] = 0x77;
        regs_[kRegNr51 - kRegNr10] = 0xF3;
    }

    // called per instruction with elapsed t-cycles
    void tick(uint32_t tcycles);
    uint8_t read_register(uint16_t addr) const;
    void write_register(uint16_t addr, uint8_t value);
    // drains stereo interleaved s16 samples, returns count written
    size_t read_audio(std::span<int16_t> out);

    // pure lfsr step, unit-tested directly
    static uint16_t lfsr_step(uint16_t lfsr, bool width7);

    // debug accessors for tests
    uint8_t debug_ch1_output() const;
    uint8_t debug_ch2_output() const;
    uint8_t debug_ch3_output() const;
    uint8_t debug_ch4_output() const;
    uint16_t debug_ch1_freq() const {
        return ch1_.freq;
    }
    uint8_t debug_ch1_volume() const {
        return ch1_.enabled ? ch1_.volume : uint8_t{0};
    }
    uint16_t debug_ch2_freq() const {
        return ch2_.freq;
    }
    uint8_t debug_ch2_volume() const {
        return ch2_.enabled ? ch2_.volume : uint8_t{0};
    }
    uint16_t debug_ch3_freq() const {
        return ch3_.freq;
    }
    bool debug_ch3_enabled() const {
        return ch3_.enabled;
    }
    uint8_t debug_ch4_volume() const {
        return ch4_.enabled ? ch4_.volume : uint8_t{0};
    }

    static constexpr size_t kStateSize = 1 + 0x17 + 0x10 + 4 + 1 + 4 + 16384 * 2 + 4 + 4 + 20 + 20 + 11 + 14;
    void save_state(StateWriter& w) const;
    void load_state(StateReader& r);

private:
    struct Square {
        bool enabled = false;
        uint16_t freq = 0;
        uint32_t timer = 0;
        uint8_t duty = 0;
        uint8_t pos = 0;
        uint16_t length = 0;
        bool length_enable = false;
        uint8_t volume = 0;
        bool env_add = false;
        uint8_t env_period = 0;
        uint8_t env_timer = 0;
        // ch1 sweep
        uint16_t sweep_shadow = 0;
        uint8_t sweep_timer = 0;
        bool sweep_enabled = false;
    };
    struct Wave {
        bool enabled = false;
        uint16_t freq = 0;
        uint32_t timer = 0;
        uint8_t pos = 0;
        uint16_t length = 0;
        bool length_enable = false;
    };
    struct Noise {
        bool enabled = false;
        uint32_t timer = 0;
        uint16_t lfsr = 0x7FFF;
        uint16_t length = 0;
        bool length_enable = false;
        uint8_t volume = 0;
        bool env_add = false;
        uint8_t env_period = 0;
        uint8_t env_timer = 0;
    };

    void save_square(StateWriter& w, const Square& ch) const;
    void load_square(StateReader& r, Square& ch);
    void step_cycle();
    void sequencer_step();
    void clock_lengths();
    void clock_envelopes();
    void clock_sweep();
    uint16_t sweep_calc() const;
    void trigger_square(Square& ch, uint8_t nrx2);
    void mix_sample();
    void push_sample(int16_t left, int16_t right);
    uint8_t reg(uint16_t addr) const {
        return regs_[addr - kRegNr10];
    }

    bool power_ = true;
    std::array<uint8_t, 0x17> regs_{};
    std::array<uint8_t, 0x10> wave_ram_{};
    Square ch1_;
    Square ch2_;
    Wave ch3_;
    Noise ch4_;
    uint32_t seq_counter_ = 0;
    uint8_t seq_step_ = 0;
    uint32_t sample_counter_ = 0;
    std::array<int16_t, 16384> ring_{};
    size_t ring_read_ = 0;
    size_t ring_count_ = 0;
};

} // namespace gb

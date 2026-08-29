#pragma once

#include "memory.hpp"

#include <array>
#include <cstdint>

// scripted flat memory for layer-2 cpu tests
class FakeBus : public gb::Memory {
public:
    uint8_t read8(uint16_t addr) override {
        return mem[addr];
    }
    void write8(uint16_t addr, uint8_t value) override {
        mem[addr] = value;
    }

    std::array<uint8_t, 0x10000> mem{};
};

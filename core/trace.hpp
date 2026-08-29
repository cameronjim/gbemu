#pragma once

#include "cpu.hpp"
#include "memory.hpp"
#include "ppu.hpp"

#include <cstdint>
#include <string>

namespace gb {

// doctor reference logs assume no ppu, so ly reads a fixed 0x90
class DoctorMemory final : public Memory {
public:
    explicit DoctorMemory(Memory& inner) : inner_(inner) {}
    uint8_t read8(uint16_t addr) override;
    void write8(uint16_t addr, uint8_t value) override;
    bool commit_speed_switch() override {
        return inner_.commit_speed_switch();
    }

private:
    Memory& inner_;
};

class Trace {
public:
    Trace() = default;
    explicit Trace(uint64_t skip_count) : skip_count_(skip_count) {}

    // appends one doctor-format line unless still skipping; call before executing
    void log(const CpuRegs& regs, Memory& bus, std::string& out);
    uint64_t count() const {
        return count_;
    }

private:
    uint64_t count_ = 0;
    uint64_t skip_count_ = 0;
};

} // namespace gb

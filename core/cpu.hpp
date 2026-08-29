#pragma once

#include "memory.hpp"
#include "state.hpp"

#include <cstddef>
#include <cstdint>

namespace gb {

inline constexpr uint8_t kFlagZ = 0x80;
inline constexpr uint8_t kFlagN = 0x40;
inline constexpr uint8_t kFlagH = 0x20;
inline constexpr uint8_t kFlagC = 0x10;

struct AluResult {
    uint8_t value;
    uint8_t flags;
};

struct Alu16Result {
    uint16_t value;
    uint8_t flags;
};

struct CpuRegs {
    // dmg post-boot values, no boot rom
    uint8_t a = 0x01;
    uint8_t f = 0xB0;
    uint8_t b = 0x00;
    uint8_t c = 0x13;
    uint8_t d = 0x00;
    uint8_t e = 0xD8;
    uint8_t h = 0x01;
    uint8_t l = 0x4D;
    uint16_t sp = 0xFFFE;
    uint16_t pc = 0x0100;
};

enum class CpuStatus : uint8_t { Running, Stopped };

class Cpu {
public:
    explicit Cpu(Memory& bus);

    // services one pending interrupt or executes one instruction, returns t-cycles
    uint32_t step();

    // applies the no-boot-rom handoff register set for the machine mode
    void set_boot_state(bool cgb_mode);

    const CpuRegs& regs() const {
        return regs_;
    }
    // mutable access is for tests and the trace logger
    CpuRegs& regs() {
        return regs_;
    }
    bool ime() const {
        return ime_;
    }
    bool halted() const {
        return halted_;
    }
    CpuStatus status() const {
        return status_;
    }
    uint8_t trap_opcode() const {
        return trap_opcode_;
    }
    uint16_t trap_pc() const {
        return trap_pc_;
    }

    static constexpr size_t kStateSize = 20;
    void save_state(StateWriter& w) const;
    void load_state(StateReader& r);

    // pure alu helpers, unit-tested directly
    static AluResult alu_add(uint8_t a, uint8_t b);
    static AluResult alu_adc(uint8_t a, uint8_t b, bool carry);
    static AluResult alu_sub(uint8_t a, uint8_t b);
    static AluResult alu_sbc(uint8_t a, uint8_t b, bool carry);
    static AluResult alu_and(uint8_t a, uint8_t b);
    static AluResult alu_xor(uint8_t a, uint8_t b);
    static AluResult alu_or(uint8_t a, uint8_t b);
    static AluResult alu_inc(uint8_t v, uint8_t flags_in);
    static AluResult alu_dec(uint8_t v, uint8_t flags_in);
    static AluResult alu_daa(uint8_t a, uint8_t flags_in);
    static AluResult alu_rlc(uint8_t v);
    static AluResult alu_rrc(uint8_t v);
    static AluResult alu_rl(uint8_t v, bool carry);
    static AluResult alu_rr(uint8_t v, bool carry);
    static AluResult alu_sla(uint8_t v);
    static AluResult alu_sra(uint8_t v);
    static AluResult alu_swap(uint8_t v);
    static AluResult alu_srl(uint8_t v);
    static uint8_t alu_bit(uint8_t v, uint8_t bit, uint8_t flags_in);
    static Alu16Result alu_add16(uint16_t a, uint16_t b, uint8_t flags_in);
    static Alu16Result alu_add_sp_e8(uint16_t sp, int8_t offset);

private:
    uint32_t execute_opcode(uint8_t opcode);
    uint32_t execute_cb(uint8_t opcode);
    uint32_t dispatch_interrupt(uint8_t pending);
    uint32_t do_halt();
    uint32_t trap_unknown(uint8_t opcode);

    uint8_t fetch_opcode();
    uint8_t fetch8();
    uint16_t fetch16();
    void push16(uint16_t value);
    uint16_t pop16();
    uint8_t pending_interrupts();

    uint16_t af() const {
        return static_cast<uint16_t>((regs_.a << 8) | regs_.f);
    }
    uint16_t bc() const {
        return static_cast<uint16_t>((regs_.b << 8) | regs_.c);
    }
    uint16_t de() const {
        return static_cast<uint16_t>((regs_.d << 8) | regs_.e);
    }
    uint16_t hl() const {
        return static_cast<uint16_t>((regs_.h << 8) | regs_.l);
    }
    uint16_t sp() const {
        return regs_.sp;
    }
    void set_af(uint16_t v) {
        regs_.a = static_cast<uint8_t>(v >> 8);
        // f low nibble always reads zero
        regs_.f = static_cast<uint8_t>(v & 0xF0);
    }
    void set_bc(uint16_t v) {
        regs_.b = static_cast<uint8_t>(v >> 8);
        regs_.c = static_cast<uint8_t>(v & 0xFF);
    }
    void set_de(uint16_t v) {
        regs_.d = static_cast<uint8_t>(v >> 8);
        regs_.e = static_cast<uint8_t>(v & 0xFF);
    }
    void set_hl(uint16_t v) {
        regs_.h = static_cast<uint8_t>(v >> 8);
        regs_.l = static_cast<uint8_t>(v & 0xFF);
    }
    bool flag_z() const {
        return (regs_.f & kFlagZ) != 0;
    }
    bool flag_c() const {
        return (regs_.f & kFlagC) != 0;
    }

    Memory& bus_;
    CpuRegs regs_;
    bool ime_ = false;
    uint8_t ime_delay_ = 0;
    bool halted_ = false;
    bool halt_bug_ = false;
    CpuStatus status_ = CpuStatus::Running;
    uint8_t trap_opcode_ = 0;
    uint16_t trap_pc_ = 0;
};

} // namespace gb

#include "cpu.hpp"

#include "interrupts.hpp"

namespace gb {

namespace {
uint8_t flags_from(bool z, bool n, bool h, bool c) {
    return static_cast<uint8_t>((z ? kFlagZ : 0) | (n ? kFlagN : 0) | (h ? kFlagH : 0) | (c ? kFlagC : 0));
}
} // namespace

Cpu::Cpu(Memory& bus) : bus_(bus) {}

void Cpu::set_boot_state(bool cgb_mode) {
    if (!cgb_mode) {
        regs_ = CpuRegs{};
        return;
    }
    // pandocs power up sequence, cgb column: a=0x11 is how games detect a cgb
    regs_.a = 0x11;
    regs_.f = 0x80;
    regs_.b = 0x00;
    regs_.c = 0x00;
    regs_.d = 0xFF;
    regs_.e = 0x56;
    regs_.h = 0x00;
    regs_.l = 0x0D;
    regs_.sp = 0xFFFE;
    regs_.pc = 0x0100;
}

uint32_t Cpu::step() {
    if (status_ != CpuStatus::Running) {
        return 0;
    }
    if (ime_delay_ > 0) {
        --ime_delay_;
        if (ime_delay_ == 0) {
            ime_ = true;
        }
    }
    const uint8_t pending = pending_interrupts();
    if (halted_) {
        if (pending == 0) {
            return 4;
        }
        halted_ = false;
    }
    if (ime_ && pending != 0) {
        return dispatch_interrupt(pending);
    }
    return execute_opcode(fetch_opcode());
}

uint32_t Cpu::dispatch_interrupt(uint8_t pending) {
    ime_ = false;
    for (uint8_t bit = 0; bit < 5; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1u << bit);
        if ((pending & mask) != 0) {
            bus_.write8(kRegIf, static_cast<uint8_t>(bus_.peek8(kRegIf) & ~mask));
            push16(regs_.pc);
            regs_.pc = kInterruptVectors[bit];
            break;
        }
    }
    // pandocs: dispatch costs 5 m-cycles
    return 20;
}

uint32_t Cpu::do_halt() {
    if (!ime_ && pending_interrupts() != 0) {
        // halt bug: pc fails to increment, next byte executes twice
        halt_bug_ = true;
        return 4;
    }
    halted_ = true;
    return 4;
}

uint32_t Cpu::do_stop() {
    // stop is two bytes; the second one is discarded
    fetch8();
    // pandocs "key1": stop is what executes an armed cgb speed switch, otherwise it does nothing
    bus_.commit_speed_switch();
    return 4;
}

uint32_t Cpu::trap_unknown(uint8_t opcode) {
    status_ = CpuStatus::Stopped;
    trap_opcode_ = opcode;
    trap_pc_ = static_cast<uint16_t>(regs_.pc - 1);
    return 4;
}

void Cpu::save_state(StateWriter& w) const {
    w.u8(regs_.a);
    w.u8(regs_.f);
    w.u8(regs_.b);
    w.u8(regs_.c);
    w.u8(regs_.d);
    w.u8(regs_.e);
    w.u8(regs_.h);
    w.u8(regs_.l);
    w.u16(regs_.sp);
    w.u16(regs_.pc);
    w.b(ime_);
    w.u8(ime_delay_);
    w.b(halted_);
    w.b(halt_bug_);
    w.u8(static_cast<uint8_t>(status_));
    w.u8(trap_opcode_);
    w.u16(trap_pc_);
}

void Cpu::load_state(StateReader& r) {
    regs_.a = r.u8();
    // f low nibble always reads zero
    regs_.f = static_cast<uint8_t>(r.u8() & 0xF0);
    regs_.b = r.u8();
    regs_.c = r.u8();
    regs_.d = r.u8();
    regs_.e = r.u8();
    regs_.h = r.u8();
    regs_.l = r.u8();
    regs_.sp = r.u16();
    regs_.pc = r.u16();
    ime_ = r.b();
    const uint8_t delay = r.u8();
    ime_delay_ = delay <= 2 ? delay : 0;
    halted_ = r.b();
    halt_bug_ = r.b();
    status_ = r.u8() == 1 ? CpuStatus::Stopped : CpuStatus::Running;
    trap_opcode_ = r.u8();
    trap_pc_ = r.u16();
}

uint8_t Cpu::pending_interrupts() {
    // internal polling, not a bus access: no time passes
    return static_cast<uint8_t>(bus_.peek8(kRegIf) & bus_.peek8(kRegIe) & kIntMask);
}

uint8_t Cpu::fetch_opcode() {
    const uint8_t opcode = bus_.read8(regs_.pc);
    if (halt_bug_) {
        halt_bug_ = false;
    } else {
        ++regs_.pc;
    }
    return opcode;
}

uint8_t Cpu::fetch8() {
    return bus_.read8(regs_.pc++);
}

uint16_t Cpu::fetch16() {
    const uint8_t lo = fetch8();
    const uint8_t hi = fetch8();
    return static_cast<uint16_t>((hi << 8) | lo);
}

void Cpu::push16(uint16_t value) {
    --regs_.sp;
    bus_.write8(regs_.sp, static_cast<uint8_t>(value >> 8));
    --regs_.sp;
    bus_.write8(regs_.sp, static_cast<uint8_t>(value & 0xFF));
}

uint16_t Cpu::pop16() {
    const uint8_t lo = bus_.read8(regs_.sp++);
    const uint8_t hi = bus_.read8(regs_.sp++);
    return static_cast<uint16_t>((hi << 8) | lo);
}

AluResult Cpu::alu_add(uint8_t a, uint8_t b) {
    const uint16_t sum = static_cast<uint16_t>(a + b);
    const uint8_t value = static_cast<uint8_t>(sum);
    return {value, flags_from(value == 0, false, ((a & 0xF) + (b & 0xF)) > 0xF, sum > 0xFF)};
}

AluResult Cpu::alu_adc(uint8_t a, uint8_t b, bool carry) {
    const uint8_t cin = carry ? 1 : 0;
    const uint16_t sum = static_cast<uint16_t>(a + b + cin);
    const uint8_t value = static_cast<uint8_t>(sum);
    // half-carry includes the carry-in
    return {value, flags_from(value == 0, false, ((a & 0xF) + (b & 0xF) + cin) > 0xF, sum > 0xFF)};
}

AluResult Cpu::alu_sub(uint8_t a, uint8_t b) {
    const uint8_t value = static_cast<uint8_t>(a - b);
    return {value, flags_from(value == 0, true, (a & 0xF) < (b & 0xF), a < b)};
}

AluResult Cpu::alu_sbc(uint8_t a, uint8_t b, bool carry) {
    const uint8_t cin = carry ? 1 : 0;
    const uint8_t value = static_cast<uint8_t>(a - b - cin);
    // half-borrow includes the borrow-in
    const bool h = (a & 0xF) < static_cast<uint16_t>((b & 0xF) + cin);
    const bool c = a < static_cast<uint16_t>(b + cin);
    return {value, flags_from(value == 0, true, h, c)};
}

AluResult Cpu::alu_and(uint8_t a, uint8_t b) {
    const uint8_t value = static_cast<uint8_t>(a & b);
    return {value, flags_from(value == 0, false, true, false)};
}

AluResult Cpu::alu_xor(uint8_t a, uint8_t b) {
    const uint8_t value = static_cast<uint8_t>(a ^ b);
    return {value, flags_from(value == 0, false, false, false)};
}

AluResult Cpu::alu_or(uint8_t a, uint8_t b) {
    const uint8_t value = static_cast<uint8_t>(a | b);
    return {value, flags_from(value == 0, false, false, false)};
}

AluResult Cpu::alu_inc(uint8_t v, uint8_t flags_in) {
    const uint8_t value = static_cast<uint8_t>(v + 1);
    const uint8_t flags = flags_from(value == 0, false, (v & 0xF) == 0xF, false);
    return {value, static_cast<uint8_t>(flags | (flags_in & kFlagC))};
}

AluResult Cpu::alu_dec(uint8_t v, uint8_t flags_in) {
    const uint8_t value = static_cast<uint8_t>(v - 1);
    const uint8_t flags = flags_from(value == 0, true, (v & 0xF) == 0, false);
    return {value, static_cast<uint8_t>(flags | (flags_in & kFlagC))};
}

AluResult Cpu::alu_daa(uint8_t a, uint8_t flags_in) {
    const bool n = (flags_in & kFlagN) != 0;
    const bool h = (flags_in & kFlagH) != 0;
    bool c = (flags_in & kFlagC) != 0;
    uint8_t adjust = 0;
    if (n) {
        if (h) {
            adjust |= 0x06;
        }
        if (c) {
            adjust |= 0x60;
        }
        a = static_cast<uint8_t>(a - adjust);
    } else {
        if (h || (a & 0xF) > 0x09) {
            adjust |= 0x06;
        }
        if (c || a > 0x99) {
            adjust |= 0x60;
            c = true;
        }
        a = static_cast<uint8_t>(a + adjust);
    }
    return {a, flags_from(a == 0, n, false, c)};
}

AluResult Cpu::alu_rlc(uint8_t v) {
    const bool c = (v & 0x80) != 0;
    const uint8_t value = static_cast<uint8_t>((v << 1) | (c ? 1 : 0));
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_rrc(uint8_t v) {
    const bool c = (v & 0x01) != 0;
    const uint8_t value = static_cast<uint8_t>((v >> 1) | (c ? 0x80 : 0));
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_rl(uint8_t v, bool carry) {
    const bool c = (v & 0x80) != 0;
    const uint8_t value = static_cast<uint8_t>((v << 1) | (carry ? 1 : 0));
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_rr(uint8_t v, bool carry) {
    const bool c = (v & 0x01) != 0;
    const uint8_t value = static_cast<uint8_t>((v >> 1) | (carry ? 0x80 : 0));
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_sla(uint8_t v) {
    const bool c = (v & 0x80) != 0;
    const uint8_t value = static_cast<uint8_t>(v << 1);
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_sra(uint8_t v) {
    const bool c = (v & 0x01) != 0;
    // arithmetic shift keeps bit 7
    const uint8_t value = static_cast<uint8_t>((v >> 1) | (v & 0x80));
    return {value, flags_from(value == 0, false, false, c)};
}

AluResult Cpu::alu_swap(uint8_t v) {
    const uint8_t value = static_cast<uint8_t>((v << 4) | (v >> 4));
    return {value, flags_from(value == 0, false, false, false)};
}

AluResult Cpu::alu_srl(uint8_t v) {
    const bool c = (v & 0x01) != 0;
    const uint8_t value = static_cast<uint8_t>(v >> 1);
    return {value, flags_from(value == 0, false, false, c)};
}

uint8_t Cpu::alu_bit(uint8_t v, uint8_t bit, uint8_t flags_in) {
    const bool z = (v & (1u << bit)) == 0;
    return static_cast<uint8_t>(flags_from(z, false, true, false) | (flags_in & kFlagC));
}

Alu16Result Cpu::alu_add16(uint16_t a, uint16_t b, uint8_t flags_in) {
    const uint32_t sum = static_cast<uint32_t>(a) + b;
    const bool h = ((a & 0x0FFF) + (b & 0x0FFF)) > 0x0FFF;
    const uint8_t flags = flags_from(false, false, h, sum > 0xFFFF);
    // z is preserved by add hl
    return {static_cast<uint16_t>(sum), static_cast<uint8_t>(flags | (flags_in & kFlagZ))};
}

Alu16Result Cpu::alu_add_sp_e8(uint16_t sp, int8_t offset) {
    const uint8_t u = static_cast<uint8_t>(offset);
    // h and c come from the unsigned low-byte add, z is always 0
    const bool h = ((sp & 0xF) + (u & 0xF)) > 0xF;
    const bool c = ((sp & 0xFF) + u) > 0xFF;
    return {static_cast<uint16_t>(sp + offset), flags_from(false, false, h, c)};
}

#include "opcodes.inc"

} // namespace gb

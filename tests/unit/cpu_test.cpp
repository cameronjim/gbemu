#include "cpu.hpp"

#include "fake_bus.hpp"
#include "interrupts.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {

constexpr uint8_t kOpNop = 0x00;
constexpr uint8_t kOpHalt = 0x76;
constexpr uint8_t kOpEi = 0xFB;
constexpr uint8_t kOpDi = 0xF3;
constexpr uint8_t kOpIncA = 0x3C;
constexpr uint8_t kOpStop = 0x10;

struct Rig {
    FakeBus bus;
    gb::Cpu cpu{bus};
};

} // namespace

TEST_CASE("ei_delays_one_instruction") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpEi;
    rig.bus.mem[0x0101] = kOpNop;
    rig.bus.mem[gb::kRegIf] = gb::kIntVBlank;
    rig.bus.mem[gb::kRegIe] = gb::kIntVBlank;
    rig.cpu.step();
    REQUIRE(!rig.cpu.ime());
    rig.cpu.step();
    // the instruction after ei still ran, no dispatch yet
    REQUIRE(rig.cpu.regs().pc == 0x0102);
    const uint32_t cycles = rig.cpu.step();
    REQUIRE(cycles == 20);
    REQUIRE(rig.cpu.regs().pc == 0x0040);
}

TEST_CASE("ei_di_fires_nothing") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpEi;
    rig.bus.mem[0x0101] = kOpDi;
    rig.bus.mem[0x0102] = kOpNop;
    rig.bus.mem[gb::kRegIf] = gb::kIntVBlank;
    rig.bus.mem[gb::kRegIe] = gb::kIntVBlank;
    rig.cpu.step();
    rig.cpu.step();
    rig.cpu.step();
    REQUIRE(!rig.cpu.ime());
    REQUIRE(rig.cpu.regs().pc == 0x0103);
    REQUIRE(rig.bus.mem[gb::kRegIf] == gb::kIntVBlank);
}

TEST_CASE("interrupt_dispatch_costs_20_tcycles_and_jumps_vector") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpEi;
    rig.bus.mem[0x0101] = kOpNop;
    rig.bus.mem[gb::kRegIf] = gb::kIntTimer;
    rig.bus.mem[gb::kRegIe] = gb::kIntTimer;
    rig.cpu.step();
    rig.cpu.step();
    const uint32_t cycles = rig.cpu.step();
    REQUIRE(cycles == 20);
    REQUIRE(rig.cpu.regs().pc == 0x0050);
    REQUIRE(!rig.cpu.ime());
    REQUIRE(rig.bus.mem[gb::kRegIf] == 0);
    REQUIRE(rig.cpu.regs().sp == 0xFFFC);
    // pushed return address is the instruction after the last executed one
    REQUIRE(rig.bus.mem[0xFFFC] == 0x02);
    REQUIRE(rig.bus.mem[0xFFFD] == 0x01);
}

TEST_CASE("interrupt_priority_order") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpEi;
    rig.bus.mem[0x0101] = kOpNop;
    rig.bus.mem[gb::kRegIf] = gb::kIntMask;
    rig.bus.mem[gb::kRegIe] = gb::kIntMask;
    rig.cpu.step();
    rig.cpu.step();
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().pc == 0x0040);
    REQUIRE(rig.bus.mem[gb::kRegIf] == (gb::kIntMask & ~gb::kIntVBlank));
    // re-enable and take the next one: stat
    rig.bus.mem[0x0040] = kOpEi;
    rig.bus.mem[0x0041] = kOpNop;
    rig.cpu.step();
    rig.cpu.step();
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().pc == 0x0048);
}

TEST_CASE("halt_wakes_without_service_when_ime_clear") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpHalt;
    rig.bus.mem[0x0101] = kOpNop;
    rig.bus.mem[gb::kRegIe] = gb::kIntVBlank;
    rig.cpu.step();
    REQUIRE(rig.cpu.halted());
    REQUIRE(rig.cpu.step() == 4);
    REQUIRE(rig.cpu.halted());
    rig.bus.mem[gb::kRegIf] = gb::kIntVBlank;
    rig.cpu.step();
    REQUIRE(!rig.cpu.halted());
    // resumed at the next instruction, no vector jump, if bit untouched
    REQUIRE(rig.cpu.regs().pc == 0x0102);
    REQUIRE(rig.bus.mem[gb::kRegIf] == gb::kIntVBlank);
}

TEST_CASE("halt_bug_repeats_next_byte") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpHalt;
    rig.bus.mem[0x0101] = kOpIncA;
    rig.bus.mem[gb::kRegIf] = gb::kIntVBlank;
    rig.bus.mem[gb::kRegIe] = gb::kIntVBlank;
    rig.cpu.step();
    REQUIRE(!rig.cpu.halted());
    rig.cpu.step();
    // pc failed to increment: inc a executed but pc still points at it
    REQUIRE(rig.cpu.regs().pc == 0x0101);
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().pc == 0x0102);
    REQUIRE(rig.cpu.regs().a == 0x03);
}

TEST_CASE("jr_taken_vs_not_taken_cycles") {
    // initial f=0xB0 has z=1
    Rig rig;
    rig.bus.mem[0x0100] = 0x20; // jr nz
    rig.bus.mem[0x0101] = 0x05;
    rig.bus.mem[0x0102] = 0x28; // jr z
    rig.bus.mem[0x0103] = 0x10;
    REQUIRE(rig.cpu.step() == 8);
    REQUIRE(rig.cpu.regs().pc == 0x0102);
    REQUIRE(rig.cpu.step() == 12);
    REQUIRE(rig.cpu.regs().pc == 0x0114);
}

TEST_CASE("jp_call_ret_taken_vs_not_taken_cycles") {
    Rig rig;
    rig.bus.mem[0x0100] = 0xC2; // jp nz, not taken
    rig.bus.mem[0x0103] = 0xCA; // jp z, taken
    rig.bus.mem[0x0104] = 0x00;
    rig.bus.mem[0x0105] = 0x02;
    REQUIRE(rig.cpu.step() == 12);
    REQUIRE(rig.cpu.step() == 16);
    REQUIRE(rig.cpu.regs().pc == 0x0200);
    rig.bus.mem[0x0200] = 0xC4; // call nz, not taken
    rig.bus.mem[0x0203] = 0xCC; // call z, taken
    rig.bus.mem[0x0204] = 0x00;
    rig.bus.mem[0x0205] = 0x03;
    REQUIRE(rig.cpu.step() == 12);
    REQUIRE(rig.cpu.step() == 24);
    REQUIRE(rig.cpu.regs().pc == 0x0300);
    REQUIRE(rig.cpu.regs().sp == 0xFFFC);
    rig.bus.mem[0x0300] = 0xC0; // ret nz, not taken
    rig.bus.mem[0x0301] = 0xC8; // ret z, taken
    REQUIRE(rig.cpu.step() == 8);
    REQUIRE(rig.cpu.step() == 20);
    REQUIRE(rig.cpu.regs().pc == 0x0206);
}

TEST_CASE("pop_af_masks_low_nibble") {
    Rig rig;
    rig.bus.mem[0x0100] = 0xF1; // pop af
    rig.cpu.regs().sp = 0xC000;
    rig.bus.mem[0xC000] = 0xFF;
    rig.bus.mem[0xC001] = 0x12;
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().a == 0x12);
    REQUIRE(rig.cpu.regs().f == 0xF0);
}

TEST_CASE("rlca_zeroes_z") {
    Rig rig;
    rig.bus.mem[0x0100] = 0x07; // rlca
    rig.cpu.regs().a = 0x00;
    rig.cpu.regs().f = 0xF0;
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().a == 0x00);
    REQUIRE(rig.cpu.regs().f == 0x00);
}

TEST_CASE("pc_wraps_at_ffff") {
    Rig rig;
    rig.cpu.regs().pc = 0xFFFF;
    rig.bus.mem[0xFFFF] = kOpNop;
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().pc == 0x0000);
}

TEST_CASE("unknown_opcode_stops_cleanly") {
    Rig rig;
    rig.bus.mem[0x0100] = 0xD3;
    rig.cpu.step();
    REQUIRE(rig.cpu.status() == gb::CpuStatus::Stopped);
    REQUIRE(rig.cpu.trap_opcode() == 0xD3);
    REQUIRE(rig.cpu.trap_pc() == 0x0100);
    // a stopped cpu makes no further progress
    REQUIRE(rig.cpu.step() == 0);
}

namespace {

struct SpeedBus final : FakeBus {
    bool commit_speed_switch() override {
        ++commits;
        return true;
    }
    uint32_t commits = 0;
};

} // namespace

TEST_CASE("stop_asks_the_bus_to_commit_a_speed_switch") {
    SpeedBus bus;
    gb::Cpu cpu(bus);
    bus.mem[0x0100] = kOpStop;
    cpu.step();
    REQUIRE(bus.commits == 1);
    // stop stays a two-byte, four-cycle instruction that keeps the cpu running
    REQUIRE(cpu.regs().pc == 0x0102);
    REQUIRE(cpu.status() == gb::CpuStatus::Running);
}

TEST_CASE("stop_is_a_plain_two_byte_nop_without_a_bus_that_switches") {
    Rig rig;
    rig.bus.mem[0x0100] = kOpStop;
    rig.bus.mem[0x0102] = kOpIncA;
    REQUIRE(rig.cpu.step() == 4);
    REQUIRE(rig.cpu.regs().pc == 0x0102);
    rig.cpu.step();
    REQUIRE(rig.cpu.regs().a == 0x02);
}

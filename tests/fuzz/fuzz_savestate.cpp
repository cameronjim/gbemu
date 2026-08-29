#include "gameboy.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

std::vector<uint8_t> make_rom(uint8_t cgb_flag) {
    std::vector<uint8_t> rom(0x8000, 0);
    rom[0x0143] = cgb_flag;
    rom[0x0147] = 0x03;
    rom[0x0149] = 0x02;
    uint8_t sum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; ++addr) {
        sum = static_cast<uint8_t>(sum - rom[addr] - 1);
    }
    rom[0x014D] = sum;
    return rom;
}

void load_and_run(const std::vector<uint8_t>& rom, std::span<const uint8_t> blob) {
    gb::Gameboy gameboy;
    if (!gameboy.load_rom(rom)) {
        return;
    }
    gameboy.load_state(blob);
    // a load must never leave a machine that cannot run
    gameboy.run_frame();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const std::vector<uint8_t> dmg_rom = make_rom(0x00);
    static const std::vector<uint8_t> cgb_rom = make_rom(0xC0);
    const std::span<const uint8_t> blob(data, size);
    // both modes: the section sizes match but the bank registers only live in cgb
    load_and_run(dmg_rom, blob);
    load_and_run(cgb_rom, blob);
    return 0;
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// builds a synthetic rom with a valid header for parse-level and facade tests
inline uint8_t test_rom_checksum(const std::vector<uint8_t>& rom) {
    uint8_t sum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; ++addr) {
        sum = static_cast<uint8_t>(sum - rom[addr] - 1);
    }
    return sum;
}

inline std::vector<uint8_t> make_test_rom(uint8_t type = 0x00, uint8_t rom_size_byte = 0x00,
                                          size_t file_size = 0x8000, uint8_t cgb_flag = 0x00) {
    std::vector<uint8_t> rom(file_size, 0);
    const std::string title = "TETRIS";
    for (size_t i = 0; i < title.size(); ++i) {
        rom[0x0134 + i] = static_cast<uint8_t>(title[i]);
    }
    rom[0x0143] = cgb_flag;
    rom[0x0147] = type;
    rom[0x0148] = rom_size_byte;
    rom[0x0149] = 0x00;
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

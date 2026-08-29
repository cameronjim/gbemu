#include "mapper_mbc5.hpp"

#include "cartridge.hpp"
#include "state.hpp"
#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace {

// each 16kb bank's first byte is its bank number, low 8 bits
std::vector<uint8_t> make_banked_rom(uint32_t banks) {
    std::vector<uint8_t> rom(static_cast<size_t>(banks) * 0x4000, 0);
    for (uint32_t b = 0; b < banks; ++b) {
        rom[static_cast<size_t>(b) * 0x4000] = static_cast<uint8_t>(b & 0xFF);
        rom[static_cast<size_t>(b) * 0x4000 + 1] = static_cast<uint8_t>(b >> 8);
    }
    return rom;
}

std::vector<uint8_t> make_mbc5_rom(uint8_t type, uint8_t ram_size_byte) {
    std::vector<uint8_t> rom = make_test_rom(type, 0x01, 0x10000, 0xC0);
    rom[0x0149] = ram_size_byte;
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

} // namespace

TEST_CASE("mbc5_nine_bit_rom_bank_split_across_2000_and_3000") {
    // 512 banks is the 8mb maximum, the only size that reaches bank 0x1ff
    gb::MapperMbc5 mapper(make_banked_rom(512), 0);
    mapper.write_rom(0x2000, 0x05);
    REQUIRE(mapper.read_rom(0x4000) == 0x05);
    REQUIRE(mapper.read_rom(0x4001) == 0x00);
    // bit 8 lives alone at 0x3000 and keeps the low byte
    mapper.write_rom(0x3000, 0x01);
    REQUIRE(mapper.read_rom(0x4000) == 0x05);
    REQUIRE(mapper.read_rom(0x4001) == 0x01);
    mapper.write_rom(0x2000, 0xFF);
    REQUIRE(mapper.read_rom(0x4000) == 0xFF);
    REQUIRE(mapper.read_rom(0x4001) == 0x01);
    // only bit 0 of the 0x3000 value is the bank bit
    mapper.write_rom(0x3000, 0xFE);
    REQUIRE(mapper.read_rom(0x4001) == 0x00);
}

TEST_CASE("mbc5_bank_zero_selectable_at_4000") {
    gb::MapperMbc5 mapper(make_banked_rom(4), 0);
    mapper.write_rom(0x2000, 0x02);
    REQUIRE(mapper.read_rom(0x4000) == 2);
    // unlike mbc1 there is no +1 quirk; bank 0 mirrors into the upper window
    mapper.write_rom(0x2000, 0x00);
    REQUIRE(mapper.read_rom(0x4000) == 0);
    REQUIRE(mapper.read_rom(0x0000) == 0);
}

TEST_CASE("mbc5_bank_masked_to_rom_size") {
    gb::MapperMbc5 mapper(make_banked_rom(8), 0);
    mapper.write_rom(0x2000, 0x0B);
    REQUIRE(mapper.read_rom(0x4000) == 3);
    mapper.write_rom(0x3000, 0x01);
    mapper.write_rom(0x2000, 0x01);
    REQUIRE(mapper.read_rom(0x4000) == 1);
}

TEST_CASE("mbc5_lower_region_is_always_bank_zero") {
    gb::MapperMbc5 mapper(make_banked_rom(8), 0);
    mapper.write_rom(0x2000, 0x03);
    REQUIRE(mapper.read_rom(0x0000) == 0);
    REQUIRE(mapper.read_rom(0x3FFF) == 0);
}

TEST_CASE("mbc5_ram_enable_gates_reads_and_writes") {
    gb::MapperMbc5 mapper(make_banked_rom(2), 0x2000);
    mapper.write_ram(0x0000, 0x42);
    REQUIRE(mapper.read_ram(0x0000) == 0xFF);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_ram(0x0000, 0x42);
    REQUIRE(mapper.read_ram(0x0000) == 0x42);
    mapper.write_rom(0x0000, 0x00);
    REQUIRE(mapper.read_ram(0x0000) == 0xFF);
    // only the low nibble decides
    mapper.write_rom(0x0000, 0xFA);
    REQUIRE(mapper.read_ram(0x0000) == 0x42);
}

TEST_CASE("mbc5_ram_banks_are_switched_by_4000") {
    // 128kb is 16 banks, the full mbc5 ram range
    gb::MapperMbc5 mapper(make_banked_rom(2), 0x20000);
    mapper.write_rom(0x0000, 0x0A);
    for (uint8_t bank = 0; bank < 16; ++bank) {
        mapper.write_rom(0x4000, bank);
        mapper.write_ram(0x0000, static_cast<uint8_t>(0xA0 + bank));
    }
    for (uint8_t bank = 0; bank < 16; ++bank) {
        mapper.write_rom(0x4000, bank);
        REQUIRE(mapper.read_ram(0x0000) == static_cast<uint8_t>(0xA0 + bank));
    }
    // bank number is 4 bits wide
    mapper.write_rom(0x4000, 0xF3);
    REQUIRE(mapper.read_ram(0x0000) == 0xA3);
    REQUIRE(mapper.external_ram().size() == 0x20000);
}

TEST_CASE("mbc5_ram_bank_masked_to_actual_ram_size") {
    gb::MapperMbc5 mapper(make_banked_rom(2), 0x2000);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_ram(0x0000, 0x77);
    mapper.write_rom(0x4000, 0x0F);
    REQUIRE(mapper.read_ram(0x0000) == 0x77);
}

TEST_CASE("mbc5_6000_region_has_no_latch_or_mode") {
    gb::MapperMbc5 mapper(make_banked_rom(8), 0x2000);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_rom(0x2000, 0x03);
    mapper.write_rom(0x6000, 0x01);
    mapper.write_rom(0x7FFF, 0xFF);
    REQUIRE(mapper.read_rom(0x0000) == 0);
    REQUIRE(mapper.read_rom(0x4000) == 3);
}

TEST_CASE("mbc5_state_roundtrips_and_remasks_tampered_bank") {
    gb::MapperMbc5 mapper(make_banked_rom(4), 0x2000);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_rom(0x2000, 0x02);
    mapper.write_rom(0x4000, 0x00);
    mapper.write_ram(0x0000, 0x5A);

    std::vector<uint8_t> blob;
    gb::StateWriter w(blob);
    mapper.save_state(w);
    REQUIRE(blob.size() == mapper.state_size());

    gb::MapperMbc5 restored(make_banked_rom(4), 0x2000);
    gb::StateReader r(blob);
    restored.load_state(r);
    REQUIRE(restored.read_rom(0x4000) == 2);
    REQUIRE(restored.read_ram(0x0000) == 0x5A);

    // out-of-range bank and ram bank bytes are re-masked on load
    blob[1] = 0xFF;
    blob[2] = 0xFF;
    blob[3] = 0xFF;
    gb::MapperMbc5 tampered(make_banked_rom(4), 0x2000);
    gb::StateReader r2(blob);
    tampered.load_state(r2);
    REQUIRE(tampered.read_rom(0x4000) == 3);
    REQUIRE(tampered.read_ram(0x0000) == 0x5A);
}

TEST_CASE("mbc5_header_types_parse_with_battery_flag") {
    struct Case {
        uint8_t type;
        bool battery;
    };
    const Case cases[] = {{0x19, false}, {0x1A, false}, {0x1B, true},
                          {0x1C, false}, {0x1D, false}, {0x1E, true}};
    for (const Case& c : cases) {
        const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(make_mbc5_rom(c.type, 0x03));
        REQUIRE(cart.has_value());
        REQUIRE(cart->type() == gb::CartType::Mbc5);
        REQUIRE(cart->has_battery() == c.battery);
        REQUIRE(cart->ram_size() == 32768u);
    }
}

TEST_CASE("mbc5_rumble_cart_banks_like_a_plain_mbc5") {
    // rumble is not emulated; 0x1d must still map and play
    std::optional<gb::Cartridge> cart = gb::Cartridge::parse(make_mbc5_rom(0x1D, 0x02));
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc5);
    gb::Mapper& mapper = cart->mapper();
    mapper.write_rom(0x2000, 0x01);
    REQUIRE(mapper.read_rom(0x0147) == 0x1D);
    mapper.write_rom(0x0000, 0x0A);
    mapper.write_ram(0x0000, 0x31);
    REQUIRE(mapper.read_ram(0x0000) == 0x31);
}

#include "cartridge.hpp"

#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> make_rom(uint8_t type = 0x00, uint8_t rom_size_byte = 0x00, size_t file_size = 0x8000) {
    return make_test_rom(type, rom_size_byte, file_size);
}

// full 16-byte title field so 0x143 is the only byte that can truncate it
std::vector<uint8_t> make_long_title_rom(uint8_t cgb_flag) {
    std::vector<uint8_t> rom = make_test_rom();
    const std::string title = "ABCDEFGHIJKLMNOP";
    for (size_t i = 0; i < title.size(); ++i) {
        rom[0x0134 + i] = static_cast<uint8_t>(title[i]);
    }
    rom[0x0143] = cgb_flag;
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

} // namespace

TEST_CASE("valid_rom_only_header_parses") {
    const std::vector<uint8_t> rom = make_rom();
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(rom);
    REQUIRE(cart.has_value());
    REQUIRE(cart->title() == "TETRIS");
    REQUIRE(cart->type() == gb::CartType::RomOnly);
    REQUIRE(cart->rom_size() == 0x8000u);
    REQUIRE(cart->ram_size() == 0u);
}

TEST_CASE("title_trims_trailing_nulls") {
    const std::vector<uint8_t> rom = make_rom();
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(rom);
    REQUIRE(cart.has_value());
    REQUIRE(cart->title().size() == 6);
}

TEST_CASE("declared_size_mismatch_rejects") {
    // header claims 64kb over a 32kb file
    const std::vector<uint8_t> rom = make_rom(0x00, 0x01, 0x8000);
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
    REQUIRE(reason == "rom size mismatch");
}

TEST_CASE("bad_checksum_rejects") {
    std::vector<uint8_t> rom = make_rom();
    rom[0x014D] ^= 0xFF;
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
    REQUIRE(reason == "header checksum mismatch");
}

TEST_CASE("unknown_cartridge_type_rejected") {
    std::string reason;
    REQUIRE(!gb::Cartridge::parse(make_rom(0x2A), &reason).has_value());
    REQUIRE(reason == "unknown cartridge type");
}

TEST_CASE("mbc_types_accepted_with_battery_flag") {
    const std::optional<gb::Cartridge> mbc1 = gb::Cartridge::parse(make_rom(0x01, 0x01, 0x10000));
    REQUIRE(mbc1.has_value());
    REQUIRE(mbc1->type() == gb::CartType::Mbc1);
    REQUIRE(!mbc1->has_battery());

    const std::optional<gb::Cartridge> mbc1b = gb::Cartridge::parse(make_rom(0x03, 0x01, 0x10000));
    REQUIRE(mbc1b.has_value());
    REQUIRE(mbc1b->type() == gb::CartType::Mbc1);
    REQUIRE(mbc1b->has_battery());

    const std::optional<gb::Cartridge> mbc3 = gb::Cartridge::parse(make_rom(0x13, 0x01, 0x10000));
    REQUIRE(mbc3.has_value());
    REQUIRE(mbc3->type() == gb::CartType::Mbc3);
    REQUIRE(mbc3->has_battery());
}

TEST_CASE("cgb_flag_parsed_from_header_0x143") {
    const std::optional<gb::Cartridge> dual = gb::Cartridge::parse(make_long_title_rom(0x80));
    REQUIRE(dual.has_value());
    REQUIRE(dual->cgb());

    const std::optional<gb::Cartridge> only = gb::Cartridge::parse(make_long_title_rom(0xC0));
    REQUIRE(only.has_value());
    REQUIRE(only->cgb());

    const std::optional<gb::Cartridge> dmg = gb::Cartridge::parse(make_long_title_rom(0x00));
    REQUIRE(dmg.has_value());
    REQUIRE(!dmg->cgb());
}

TEST_CASE("cgb_flag_byte_is_not_part_of_the_title") {
    const std::optional<gb::Cartridge> cgb = gb::Cartridge::parse(make_long_title_rom(0x80));
    REQUIRE(cgb.has_value());
    REQUIRE(cgb->title() == "ABCDEFGHIJKLMNO");

    // any other 0x143 value stays a title byte
    const std::optional<gb::Cartridge> dmg = gb::Cartridge::parse(make_long_title_rom('P'));
    REQUIRE(dmg.has_value());
    REQUIRE(dmg->title() == "ABCDEFGHIJKLMNOP");
}

TEST_CASE("dmg_title_detection_unchanged") {
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(make_rom());
    REQUIRE(cart.has_value());
    REQUIRE(cart->title() == "TETRIS");
    REQUIRE(!cart->cgb());
}

TEST_CASE("truncated_file_rejects") {
    for (size_t size : {size_t{0}, size_t{0x100}, size_t{0x14F}}) {
        const std::vector<uint8_t> rom(size, 0);
        std::string reason;
        REQUIRE(!gb::Cartridge::parse(rom, &reason).has_value());
        REQUIRE(reason == "file smaller than header");
    }
}

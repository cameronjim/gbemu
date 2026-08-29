#include "cartridge.hpp"

#include "mapper_mbc1.hpp"
#include "mapper_mbc3.hpp"
#include "mapper_mbc5.hpp"
#include "mapper_rom.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace gb {

namespace {
constexpr uint16_t kHeaderTitleStart = 0x0134;
constexpr uint16_t kHeaderTitleEnd = 0x0143;
constexpr uint16_t kHeaderCgbFlag = 0x0143;
constexpr uint8_t kCgbFlagDual = 0x80;
constexpr uint8_t kCgbFlagOnly = 0xC0;
constexpr uint16_t kHeaderType = 0x0147;
constexpr uint16_t kHeaderRomSize = 0x0148;
constexpr uint16_t kHeaderRamSize = 0x0149;
constexpr uint16_t kHeaderChecksumLast = 0x014C;
constexpr uint16_t kHeaderChecksum = 0x014D;
constexpr size_t kMinRomSize = 0x0150;
} // namespace

std::optional<Cartridge> Cartridge::parse(std::span<const uint8_t> bytes, std::string* reject_reason) {
    const auto reject = [reject_reason](const char* why) {
        if (reject_reason != nullptr) {
            *reject_reason = why;
        }
        return std::nullopt;
    };

    if (bytes.size() < kMinRomSize) {
        return reject("file smaller than header");
    }

    uint8_t sum = 0;
    for (uint16_t addr = kHeaderTitleStart; addr <= kHeaderChecksumLast; ++addr) {
        // pandocs: checksum folds each byte as x = x - byte - 1
        sum = static_cast<uint8_t>(sum - bytes[addr] - 1);
    }
    if (sum != bytes[kHeaderChecksum]) {
        return reject("header checksum mismatch");
    }

    const uint8_t rom_size_byte = bytes[kHeaderRomSize];
    if (rom_size_byte > 0x08) {
        return reject("unknown rom size");
    }
    // pandocs: rom size is 32 kib << value
    const uint32_t declared_rom = 32768u << rom_size_byte;
    if (static_cast<size_t>(declared_rom) != bytes.size()) {
        return reject("rom size mismatch");
    }

    const uint8_t type = bytes[kHeaderType];
    const bool mbc1 = type >= 0x01 && type <= 0x03;
    const bool mbc3 = type >= 0x0F && type <= 0x13;
    // 0x1c-0x1e are the rumble variants; rumble itself is not emulated
    const bool mbc5 = type >= 0x19 && type <= 0x1E;
    if (type != 0x00 && !mbc1 && !mbc3 && !mbc5) {
        return reject("unknown cartridge type");
    }

    uint32_t ram_size = 0;
    switch (bytes[kHeaderRamSize]) {
    case 0x00:
        ram_size = 0;
        break;
    case 0x02:
        ram_size = 8192;
        break;
    case 0x03:
        ram_size = 32768;
        break;
    case 0x04:
        ram_size = 131072;
        break;
    case 0x05:
        ram_size = 65536;
        break;
    default:
        return reject("unknown ram size");
    }

    const uint8_t cgb_flag = bytes[kHeaderCgbFlag];
    const bool cgb = cgb_flag == kCgbFlagDual || cgb_flag == kCgbFlagOnly;
    // pandocs: 0x143 doubles as the cgb flag, so a cgb title stops one byte earlier
    const uint16_t title_end = cgb ? static_cast<uint16_t>(kHeaderTitleEnd - 1) : kHeaderTitleEnd;

    std::string title;
    for (uint16_t addr = kHeaderTitleStart; addr <= title_end; ++addr) {
        if (bytes[addr] == 0) {
            break;
        }
        title.push_back(static_cast<char>(bytes[addr]));
    }

    Cartridge cart;
    cart.title_ = std::move(title);
    cart.cgb_ = cgb;
    cart.rom_size_ = declared_rom;
    cart.ram_size_ = ram_size;
    cart.has_battery_ =
        type == 0x03 || type == 0x0F || type == 0x10 || type == 0x13 || type == 0x1B || type == 0x1E;
    std::vector<uint8_t> rom(bytes.begin(), bytes.end());
    if (mbc1) {
        cart.type_ = CartType::Mbc1;
        cart.mapper_ = std::make_unique<MapperMbc1>(std::move(rom), ram_size);
    } else if (mbc3) {
        cart.type_ = CartType::Mbc3;
        cart.mapper_ = std::make_unique<MapperMbc3>(std::move(rom), ram_size);
    } else if (mbc5) {
        cart.type_ = CartType::Mbc5;
        cart.mapper_ = std::make_unique<MapperMbc5>(std::move(rom), ram_size);
    } else {
        cart.type_ = CartType::RomOnly;
        cart.mapper_ = std::make_unique<MapperRom>(std::move(rom));
    }
    return cart;
}

} // namespace gb

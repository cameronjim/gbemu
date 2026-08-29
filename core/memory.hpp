#pragma once

#include <cstdint>

namespace gb {

// memory seam between cpu and mediator; the concrete dmg bus lands in milestone 04
class Memory {
public:
    virtual ~Memory() = default;
    virtual uint8_t read8(uint16_t addr) = 0;
    virtual void write8(uint16_t addr, uint8_t value) = 0;
    // side-effect-free read: no time passes, used for interrupt polling and tracing
    virtual uint8_t peek8(uint16_t addr) {
        return read8(addr);
    }
    // stop executes an armed cgb speed switch; false means nothing was armed
    virtual bool commit_speed_switch() {
        return false;
    }
};

} // namespace gb

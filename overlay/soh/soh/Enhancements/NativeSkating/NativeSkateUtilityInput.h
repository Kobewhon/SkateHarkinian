#pragma once
#include <cstdint>
namespace NativeSkateUtility {
// Stock logical BTN_L/B/DUP/DDOWN; independent of physical controller.
constexpr uint16_t L = 0x0020, Circle = 0x4000, Up = 0x0800, Down = 0x0400;
struct Input {
    uint16_t previous = 0, blocked = 0;
    bool held = false;
    struct Commands {
        bool set = false, load = false, toggle = false;
    };
    Commands Read(uint16_t buttons, uint16_t pressed, bool active, bool editor) {
        blocked &= buttons;
        held = active && (buttons & L);
        uint16_t fresh = (buttons & ~previous) | pressed;
        previous = buttons;
        Commands result;
        if (held) {
            uint16_t supported = editor ? Circle : uint16_t(Circle | Up | Down);
            uint16_t commands = fresh & ~blocked & supported;
            blocked |= (buttons & supported) | commands;
            result.toggle = commands & Circle;
            if (!editor && !result.toggle) {
                result.set = commands & Down;
                result.load = commands & Up;
            }
        }
        return result;
    }
    uint16_t Mask() const {
        return blocked | (held ? L : 0);
    }
    uint16_t HostMask(bool editor, bool toggle) const {
        return (editor ? 0 : L) | (editor ? (Mask() & ~L) : Mask()) | (toggle ? L : 0);
    }
    void Reset() {
        previous = blocked = 0;
        held = false;
    }
};
} // namespace NativeSkateUtility

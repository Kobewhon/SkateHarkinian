#pragma once
#include <cstdint>
namespace NativeSkateContextInput {
// One press has one classification. Time is the authoritative 60 Hz native tick.
struct Action {
    static constexpr uint64_t HoldTicks = 11; // 183.3 ms
    bool down = false, brake = false, eligible = false;
    unsigned context = 0;
    uint64_t began = 0;
    struct Result {
        bool attack = false, brake = false;
    };
    Result Read(bool held, bool onboard, bool drawn, bool safe, uint64_t tick) {
        bool attack = false;
        const unsigned now = (onboard ? 1u : 0u) | (drawn ? 2u : 0u);
        if (held && !down) {
            began = tick;
            eligible = safe;
            brake = false;
            context = now;
        }
        if (down && now != context)
            eligible = false;
        if (held) {
            eligible = eligible && safe;
            if (!onboard) {
                attack = !down && safe;
                brake = false;
            } else if (!drawn)
                brake = true;
            else if (tick - began >= HoldTicks)
                brake = true;
        } else {
            attack = down && onboard && drawn && !brake && eligible && safe && now == context;
            brake = false;
            eligible = false;
        }
        down = held;
        return { attack, brake };
    }
    void Reset() {
        *this = {};
    }
};
struct DryGate {
    uint64_t first = 0, last = 0;
    bool stable = false;
    bool Read(bool valid, uint64_t tick) {
        if (!valid) {
            stable = false;
            first = last = tick;
            return false;
        }
        if (!stable || tick < last || tick - last > 6) {
            first = tick;
            stable = true;
        }
        last = tick;
        return tick - first >= 24; // 400 ms grounded and dry
    }
    void Reset() {
        *this = {};
    }
};
} // namespace NativeSkateContextInput

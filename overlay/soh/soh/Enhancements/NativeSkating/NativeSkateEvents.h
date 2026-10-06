#pragma once
#include <cstdint>
namespace NativeSkateRuntime {
enum class EventPhase : uint32_t { Start = 1, Update = 2, Complete = 3, Land = 4, Bail = 5, GrindExit = 6, Reset = 7 };
struct Event {
    uint64_t sequence, tick;
    uint32_t phase, category;
    int32_t id;
    uint32_t flags;
    float duration, current, combo, multiplier, banked;
    char name[96];
};
static_assert(sizeof(Event) == 152, "Native fixed-tick event ABI");
} // namespace NativeSkateRuntime

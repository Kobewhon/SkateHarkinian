#pragma once
#include "NativeSkateRetargetMath.h"
#include <cstdint>
namespace NativeSkateContactMath {
using namespace NativeSkateRetargetMath;
struct Leg {
    V knee, ankle;
    bool reachable;
};
inline Leg Solve(V hip, V knee, V target, float upper, float lower, V fallback) {
    V delta = target - hip;
    float distance = Length(delta);
    V direction = distance > 1e-5f ? delta * (1 / distance) : V{ 0, -1, 0 };
    float minimum = std::abs(upper - lower) + .001f, maximum = std::max(minimum, upper + lower - .001f);
    float reach = std::clamp(distance, minimum, maximum);
    V bend = knee - hip;
    bend = bend - direction * Dot(bend, direction);
    if (Length(bend) < 1e-4f) {
        bend = fallback - direction * Dot(fallback, direction);
        if (Length(bend) < 1e-4f)
            bend = Cross(direction, V{ 1, 0, 0 });
    }
    bend = Unit(bend);
    float along = (upper * upper - lower * lower + reach * reach) / (2 * reach),
          height = std::sqrt(std::max(0.f, upper * upper - along * along));
    return { hip + direction * along + bend * height, hip + direction * reach,
             distance >= minimum && distance <= maximum };
}
// Feasible common pelvis translation along the board normal. Each actual
// chain contributes an interval; choosing their intersection keeps both legs
// reachable without modifying either bone length.
inline bool ReachInterval(V hip, V ankle, V normal, float reach, float& low, float& high) {
    V d = ankle - hip;
    float along = Dot(d, normal), side = std::max(0.f, Dot(d, d) - along * along);
    float remaining = reach * reach - side;
    if (remaining < 0)
        return false;
    float vertical = std::sqrt(remaining);
    low = std::max(low, along - vertical);
    high = std::min(high, along + vertical);
    return low <= high;
}
inline float Weight(float previous, float requested, uint64_t ticks) {
    float dt = std::min(ticks / 60.f, .1f);
    return previous + (requested - previous) * (1 - std::exp(-dt / .065f));
}
// Actual stock NearDL sole patches measured in ankle-local model units; Child is not scaled Adult.
inline V Sole(bool adult, int side) {
    return adult ? (side == 0 ? V{ -224.36364f, 522.18182f, -72.90909f } : V{ 235.f, 524.80769f, -16.73077f })
                 : (side == 0 ? V{ -103.63636f, 310.63636f, 69.81818f } : V{ 68.52632f, 313.05263f, 5.73684f });
}
inline float DeckTopMeters(float boardForwardMeters) {
    float rise = std::clamp((std::abs(boardForwardMeters) - .27f) / .13f, 0.f, 1.f);
    return .01f + .027f * rise * rise;
}
} // namespace NativeSkateContactMath

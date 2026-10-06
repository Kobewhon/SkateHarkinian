#pragma once
#include <cmath>

namespace NativeSkateUnits {
constexpr float kOotUnitsPerMeter = 54.0f / 0.885f;
// Mesh lengths are distinct from world positions. A converted HostLength
// cannot be passed back into RustLengthToHost without an explicit type change.
struct RustMeters {
    float value;
};
struct HostLength {
    float value;
};
constexpr HostLength RustLengthToHost(RustMeters length) {
    return { length.value * kOotUnitsPerMeter };
}
struct Anchor {
    float rust[3] = {};
    float oot[3] = {};
    bool valid = false;
    float unitsPerMeter = kOotUnitsPerMeter;
    void Set(const float source[3], const float host[3]) {
        valid = source && host;
        for (int i = 0; i < 3 && valid; ++i) {
            valid = std::isfinite(source[i]) && std::isfinite(host[i]);
            rust[i] = source[i];
            oot[i] = host[i];
        }
    }
    bool Position(const float source[3], float host[3]) const {
        if (!valid || !source || !host)
            return false;
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(source[i]))
                return false;
            host[i] = oot[i] + (source[i] - rust[i]) * unitsPerMeter;
        }
        return true;
    }
    static bool ToRust(const float host[3], float source[3]) {
        if (!host || !source)
            return false;
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(host[i]))
                return false;
            source[i] = host[i] / kOotUnitsPerMeter;
        }
        return true;
    }
};
} // namespace NativeSkateUnits

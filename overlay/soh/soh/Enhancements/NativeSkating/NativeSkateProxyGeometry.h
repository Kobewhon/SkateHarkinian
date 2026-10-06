#pragma once
#include "NativeSkateUnits.h"
#include <cmath>

namespace NativeSkateProxyGeometry {
constexpr NativeSkateUnits::RustMeters kDeckLength{ 0.80f };
constexpr NativeSkateUnits::RustMeters kDeckWidth{ 0.20f };
constexpr NativeSkateUnits::RustMeters kDeckThickness{ 0.04f };
constexpr auto kDeckLengthHost = NativeSkateUnits::RustLengthToHost(kDeckLength);
constexpr auto kDeckWidthHost = NativeSkateUnits::RustLengthToHost(kDeckWidth);
constexpr auto kDeckThicknessHost = NativeSkateUnits::RustLengthToHost(kDeckThickness);
struct Shape {
    float radius = .03f, wheelY = -.0565f, wheelX = .095f, frontZ = .242f, backZ = -.242f;
};
inline Shape NativeShape(const float* d) {
    Shape s;
    if (!d)
        return s;
    if (std::isfinite(d[19]) && d[19] > .005f && d[19] < .1f && std::isfinite(d[20]) && std::abs(d[20]) < .2f &&
        std::isfinite(d[21]) && d[21] > .02f && d[21] < .2f && std::isfinite(d[22]) && std::isfinite(d[23]) &&
        std::abs(d[22]) < .5f && std::abs(d[23]) < .5f)
        s = { d[19], d[20], d[21], d[22], d[23] };
    return s;
}

// Host-sized mesh vertices require a rotation-only parent, never meter scale.
inline bool BoardRotation(const float source[16], float output[16]) {
    for (int i = 0; i < 16; ++i)
        output[i] = 0.0f;
    output[15] = 1.0f;
    for (int col = 0; col < 3; ++col) {
        float squared = 0;
        for (int row = 0; row < 3; ++row) {
            const float v = source[col * 4 + row];
            if (!std::isfinite(v))
                return false;
            squared += v * v;
        }
        if (!std::isfinite(squared) || squared < 1e-8f)
            return false;
        const float inverse = 1.0f / std::sqrt(squared);
        for (int row = 0; row < 3; ++row)
            output[col * 4 + row] = source[col * 4 + row] * inverse;
    }
    const float determinant = output[0] * (output[5] * output[10] - output[6] * output[9]) -
                              output[4] * (output[1] * output[10] - output[2] * output[9]) +
                              output[8] * (output[1] * output[6] - output[2] * output[5]);
    return std::isfinite(determinant) && determinant > 0.99f;
}
} // namespace NativeSkateProxyGeometry

#pragma once
#include "NativeSkateRiderScale.h"
#include <cmath>
#include <cstdint>
namespace NativeSkateLink {
struct Frame {
    float position[3] = {};
    float foot[3] = {};
    float heading = 0;
    int16_t yaw = 0;
    bool adult = true, valid = false;
};
// Measured soles of the ordinary normal_wait_free frame 0, at OoT scale .01.
constexpr float kAdultSole = -.2275604f, kChildSole = -.7351763f;
inline Frame Build(const float com[3], float heading, float spatialScale, bool adult, float modelScaleY = .01f) {
    Frame f;
    f.adult = adult;
    if (!com || !std::isfinite(heading) || !std::isfinite(spatialScale) || spatialScale <= 0 ||
        !std::isfinite(modelScaleY) || modelScaleY <= 0)
        return f;
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(com[i]) || std::abs(com[i]) > 1e7f)
            return f;
        f.position[i] = f.foot[i] = com[i];
    }
    f.foot[1] += NativeSkateRiderScale::kFoot / spatialScale;
    f.position[1] = f.foot[1] - (adult ? kAdultSole : kChildSole) * (modelScaleY / .01f);
    // Native and OoT yaw both face +Z at zero and +X at +pi/2.
    f.heading = std::remainder(heading, 6.2831853071795864769f);
    auto binary = static_cast<int32_t>(std::lrint(f.heading * (32768.f / 3.14159265358979323846f)));
    if (binary >= 32768)
        binary -= 65536;
    if (binary < -32768)
        binary += 65536;
    f.yaw = static_cast<int16_t>(binary);
    f.valid = true;
    return f;
}
} // namespace NativeSkateLink

#pragma once
#include <cmath>
#include <algorithm>
namespace NativeSkateRiderScale {
// Block-proxy bounds, feet=-58, head=+35.5 relative to COM.
constexpr float kFoot = -58.f, kHead = 35.5f, kHeight = kHead - kFoot;
// Local original OoT assets, normal_wait_free frame0, hierarchy + vertex bounds,
// actor scale .01 and child root translation .64; no equipment additions.
constexpr float kAdultHeight = 62.165557f, kChildHeight = 43.343237f;
struct Settings {
    int mode;
    float multiplier, target, finalHeight, ratio;
};
inline Settings Get(int mode, float multiplier, float spatialScale) {
    if (mode < 0 || mode > 2)
        mode = 1;
    if (!std::isfinite(multiplier))
        multiplier = 1;
    multiplier = std::clamp(multiplier, .25f, 2.f);
    if (!std::isfinite(spatialScale) || spatialScale <= 0)
        spatialScale = 1;
    float height = kHeight / spatialScale;
    float target = mode == 1 ? kAdultHeight : mode == 2 ? kChildHeight : height;
    return { mode, multiplier, target, target * multiplier, target * multiplier / height };
}
inline float PositionY(float localY, const Settings& s) {
    return kFoot + (localY - kFoot) * s.ratio;
}
} // namespace NativeSkateRiderScale

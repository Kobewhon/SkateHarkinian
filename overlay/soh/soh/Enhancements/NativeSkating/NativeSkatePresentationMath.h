#pragma once
#include <algorithm>
#include <cmath>
namespace NativeSkatePresentationMath {
inline float Bound(float v, float fallback, float lo, float hi) {
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}
inline float Smooth(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
struct BipedJumpPose {
    float tuck, compression, bend, torso, freeArm, carryArm;
};
inline BipedJumpPose JumpPose(bool air, float flight, float landing, bool carry) {
    // Keep a visible airborne pose until actual contact, rather than ending an
    // assumed .65-second jump while the native rider is still in BipedAir.
    float enter = std::clamp(flight / .12f, 0.f, 1.f);
    enter = enter * enter * (3 - 2 * enter);
    float landBlend = std::clamp(landing / .12f, 0.f, 1.f);
    landBlend = landBlend * landBlend * (3 - 2 * landBlend);
    float tuck =
        air ? enter * (.65f + .35f * std::exp(-std::pow((flight - .45f) / .18f, 2.f))) : .65f * (1 - landBlend);
    float compression = air ? std::max(0.f, 1 - flight / .10f) * .4f
                            : std::max(0.f, std::sin(std::clamp(landing / .22f, 0.f, 1.f) * 3.14159265f));
    return { tuck,
             compression,
             .30f * tuck + .38f * compression,
             (carry ? .10f : .16f) * tuck + .12f * compression,
             (carry ? .30f : .18f) * tuck + .10f * compression,
             carry ? .12f * tuck + .08f * compression : 0 };
}
struct JumpBaseFrame {
    int kind = 0;
    float progress = 0;
};
inline JumpBaseFrame JumpBase(bool air, float flight, float landing, float speed) {
    if (air) {
        float p = flight < .20f ? .5f * Smooth(flight / .20f) : .5f + .5f * Smooth((flight - .60f) / .40f);
        return { speed > 2.4f ? 2 : 1, p };
    }
    if (landing < .30f)
        return { 3, Smooth(landing / .30f) };
    return {};
}
inline bool JumpVoice(int before, int after, float vertical) {
    return before == 500 && after == 501 && std::isfinite(vertical) && vertical > .01f;
}
inline int BipedAnimation(int current, float speed) {
    if (!std::isfinite(speed))
        return 0;
    if (current == 0)
        return speed > .15f ? (speed > 2.4f ? 2 : 1) : 0;
    if (speed < .08f)
        return 0;
    if (current == 2)
        return speed < 2.0f ? 1 : 2;
    return speed > 2.4f ? 2 : 1;
}
inline int BipedJumpAnimation(int current, float speed, bool airborne) {
    return airborne ? 0 : BipedAnimation(current, speed);
}

struct Q {
    float x = 0, y = 0, z = 0, w = 1;
};
inline Q Normalize(Q q) {
    float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return n > 1e-6f && std::isfinite(n) ? Q{ q.x / n, q.y / n, q.z / n, q.w / n } : Q{};
}
// Column-major orthonormal host transform; units only affect translation.
inline Q Rotation(const float* m) {
    Q q;
    float trace = m[0] + m[5] + m[10];
    if (trace > 0) {
        float s = std::sqrt(trace + 1) * 2;
        q = { (m[6] - m[9]) / s, (m[8] - m[2]) / s, (m[1] - m[4]) / s, s / 4 };
    } else if (m[0] > m[5] && m[0] > m[10]) {
        float s = std::sqrt(1 + m[0] - m[5] - m[10]) * 2;
        q = { s / 4, (m[4] + m[1]) / s, (m[8] + m[2]) / s, (m[6] - m[9]) / s };
    } else if (m[5] > m[10]) {
        float s = std::sqrt(1 + m[5] - m[0] - m[10]) * 2;
        q = { (m[4] + m[1]) / s, s / 4, (m[9] + m[6]) / s, (m[8] - m[2]) / s };
    } else {
        float s = std::sqrt(1 + m[10] - m[0] - m[5]) * 2;
        q = { (m[8] + m[2]) / s, (m[9] + m[6]) / s, s / 4, (m[1] - m[4]) / s };
    }
    return Normalize(q);
}
inline void Blend(const float* a, const float* b, float t, float* out) {
    Q x = Rotation(a), y = Rotation(b);
    float dot = x.x * y.x + x.y * y.y + x.z * y.z + x.w * y.w;
    if (dot < 0)
        y = { -y.x, -y.y, -y.z, -y.w };
    Q q = Normalize({ x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t });
    for (int i = 0; i < 16; ++i)
        out[i] = 0;
    out[15] = 1;
    out[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
    out[1] = 2 * (q.x * q.y + q.z * q.w);
    out[2] = 2 * (q.x * q.z - q.y * q.w);
    out[4] = 2 * (q.x * q.y - q.z * q.w);
    out[5] = 1 - 2 * (q.x * q.x + q.z * q.z);
    out[6] = 2 * (q.y * q.z + q.x * q.w);
    out[8] = 2 * (q.x * q.z + q.y * q.w);
    out[9] = 2 * (q.y * q.z - q.x * q.w);
    out[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
    for (int i = 12; i < 15; ++i)
        out[i] = a[i] + (b[i] - a[i]) * t;
}
} // namespace NativeSkatePresentationMath

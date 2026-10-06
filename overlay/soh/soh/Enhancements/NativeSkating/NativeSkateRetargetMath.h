#pragma once
#include <cmath>
#include <algorithm>
namespace NativeSkateRetargetMath {
struct V {
    float x = 0, y = 0, z = 0;
    V operator+(V b) const {
        return { x + b.x, y + b.y, z + b.z };
    }
    V operator-(V b) const {
        return { x - b.x, y - b.y, z - b.z };
    }
    V operator*(float s) const {
        return { x * s, y * s, z * s };
    }
};
inline float Dot(V a, V b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline V Cross(V a, V b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline float Length(V a) {
    return std::sqrt(Dot(a, a));
}
inline V Unit(V a) {
    float n = Length(a);
    return n > 1e-6f ? a * (1 / n) : V{};
}
struct M {
    float v[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
};
inline M Inverse(M a) {
    M b;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            b.v[i][j] = a.v[j][i];
    return b;
}
inline M Mul(M a, M b) {
    M r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            r.v[i][j] = 0;
            for (int k = 0; k < 3; ++k)
                r.v[i][j] += a.v[i][k] * b.v[k][j];
        }
    return r;
}
inline V Mul(M a, V b) {
    return { a.v[0][0] * b.x + a.v[0][1] * b.y + a.v[0][2] * b.z, a.v[1][0] * b.x + a.v[1][1] * b.y + a.v[1][2] * b.z,
             a.v[2][0] * b.x + a.v[2][1] * b.y + a.v[2][2] * b.z };
}
inline bool Basis(const float* a, M& out) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(a[i]))
            return false;
    V x = { a[0], a[1], a[2] }, y = { a[4], a[5], a[6] }, z = { a[8], a[9], a[10] };
    if (Length(x) < .1f || Length(y) < .1f || Length(z) < .1f || Length(x) > 10 || Length(y) > 10 || Length(z) > 10 ||
        Dot(Cross(x, y), z) <= 1e-5f)
        return false;
    x = Unit(x);
    y = Unit(y - x * Dot(x, y));
    if (Length(y) < .9f)
        return false;
    z = Cross(x, y);
    for (int i = 0; i < 3; ++i) {
        out.v[i][0] = i == 0 ? x.x : i == 1 ? x.y : x.z;
        out.v[i][1] = i == 0 ? y.x : i == 1 ? y.y : y.z;
        out.v[i][2] = i == 0 ? z.x : i == 1 ? z.y : z.z;
    }
    return true;
}
inline M Euler(float x, float y, float z) {
    float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y), sz = std::sin(z), cz = std::cos(z);
    M r;
    float a[3][3] = { { cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz },
                      { cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz },
                      { -sy, sx * cy, cx * cy } };
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.v[i][j] = a[i][j];
    return r;
}
inline V Euler(M r) {
    float y = std::asin(std::clamp(-r.v[2][0], -1.f, 1.f));
    if (std::abs(std::cos(y)) > 1e-5f)
        return { std::atan2(r.v[2][1], r.v[2][2]), y, std::atan2(r.v[1][0], r.v[0][0]) };
    return { 0, y, std::atan2(-r.v[0][1], r.v[1][1]) };
}
inline M Swing(V from, V to) {
    from = Unit(from);
    to = Unit(to);
    V cross = Cross(from, to);
    float c = Dot(from, to);
    if (c > .99999f)
        return {};
    if (c < -.99999f) {
        V axis = Unit(Cross(from, std::abs(from.x) < .8f ? V{ 1, 0, 0 } : V{ 0, 1, 0 }));
        M r;
        float a[] = { axis.x, axis.y, axis.z };
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.v[i][j] = 2 * a[i] * a[j] - (i == j ? 1.f : 0.f);
        return r;
    }
    float k[3][3] = { { 0, -cross.z, cross.y }, { cross.z, 0, -cross.x }, { -cross.y, cross.x, 0 } };
    M r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float k2 = 0;
            for (int n = 0; n < 3; ++n)
                k2 += k[i][n] * k[n][j];
            r.v[i][j] = (i == j ? 1.f : 0.f) + k[i][j] + k2 / (1 + c);
        }
    return r;
}
inline M Retarget(M linkRest, M correction, M nativeRest, M nativeCurrent) {
    return Mul(Mul(Mul(linkRest, correction), Mul(Inverse(nativeRest), nativeCurrent)), Inverse(correction));
}
} // namespace NativeSkateRetargetMath

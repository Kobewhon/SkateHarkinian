#pragma once
#include "NativeSkateRetargetMath.h"
#include <cstdint>
namespace NativeSkateBoardPresentation {
enum class Mode { PHYSICS_BOARD, HELD_BOARD, CARRIED_BOARD, AIR_BOARD };
// Canonical PhysicalStateId: BipedGround=500, BipedAir=501.
inline bool Biped(uint32_t state) {
    return state == 500 || state == 501;
}
inline bool Carry(uint32_t state, bool held) {
    return Biped(state) && held;
}
inline const char* Name(Mode m) {
    return m == Mode::HELD_BOARD      ? "HELD_BOARD"
           : m == Mode::CARRIED_BOARD ? "CARRIED_BOARD"
           : m == Mode::AIR_BOARD     ? "AIR_BOARD"
                                      : "PHYSICS_BOARD";
}
// Rigid transforms only: a transition's start pose follows the CURRENT rider
// frame instead of remaining frozen in last frame's absolute world position.
inline bool Relative(const float* frame, const float* world, float* out) {
    using namespace NativeSkateRetargetMath;
    M f, w;
    if (!Basis(frame, f) || !Basis(world, w))
        return false;
    M inv = Inverse(f), rot = Mul(inv, w);
    V p = Mul(inv, V{ world[12] - frame[12], world[13] - frame[13], world[14] - frame[14] });
    std::fill_n(out, 16, 0.f);
    out[15] = 1;
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row)
            out[c * 4 + row] = rot.v[row][c];
    out[12] = p.x;
    out[13] = p.y;
    out[14] = p.z;
    return true;
}
inline bool World(const float* frame, const float* local, float* out) {
    using namespace NativeSkateRetargetMath;
    M f, l;
    if (!Basis(frame, f) || !Basis(local, l))
        return false;
    M rot = Mul(f, l);
    V p = Mul(f, V{ local[12], local[13], local[14] }) + V{ frame[12], frame[13], frame[14] };
    std::fill_n(out, 16, 0.f);
    out[15] = 1;
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row)
            out[c * 4 + row] = rot.v[row][c];
    out[12] = p.x;
    out[13] = p.y;
    out[14] = p.z;
    return true;
}
// Retarget an object relative to calibrated BODY frames, not deck parenting.
// Positions have already crossed the ONE meters->host boundary. Mesh size is
// independent; scale changes only rider-relative translation.
inline bool Convert(const float* nativeBody, const float* linkBody, const float* nativeBoard,
                    const float* nativeBoardHostPosition, const float* linkPelvis, float scale, float out[16]) {
    using namespace NativeSkateRetargetMath;
    M source, target, board;
    if (!Basis(nativeBody, source) || !Basis(linkBody, target) || !Basis(nativeBoard, board) || !std::isfinite(scale) ||
        scale <= 0 || scale > 2)
        return false;
    V delta = { nativeBoardHostPosition[0] - nativeBody[12], nativeBoardHostPosition[1] - nativeBody[13],
                nativeBoardHostPosition[2] - nativeBody[14] };
    V local = Mul(Inverse(source), delta) * scale;
    V pos = Mul(target, local);
    M rotation = Mul(Mul(target, Inverse(source)), board);
    for (int i = 0; i < 16; ++i)
        out[i] = 0;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            out[col * 4 + row] = rotation.v[row][col];
    for (int a = 0; a < 3; ++a)
        out[12 + a] = linkPelvis[a] + (a == 0 ? pos.x : a == 1 ? pos.y : pos.z);
    out[15] = 1;
    return std::all_of(out, out + 16, [](float v) { return std::isfinite(v); });
}
} // namespace NativeSkateBoardPresentation

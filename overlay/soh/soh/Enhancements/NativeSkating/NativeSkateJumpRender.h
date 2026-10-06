#pragma once
#include "NativeSkatePresentationMath.h"
// Called by the final Player_DrawImpl limb override, using actual PlayerLimb
// IDs. Link leg bones extend along local X: knee/hip flex is local Z, not X.
namespace NativeSkateJumpRender {
template <class Position, class Rotation>
inline void Apply(int limb, Position& pos, Rotation& rot, const NativeSkatePresentationMath::BipedJumpPose& jump,
                  bool carry) {
    auto angle = [](float v) { return static_cast<short>(std::lround(v * 32768 / 3.14159265f)); };
    if (limb == PLAYER_LIMB_ROOT)
        pos.y -= 110 * jump.compression;
    if (limb == PLAYER_LIMB_WAIST)
        rot.z += angle(.08f * jump.tuck);
    if (limb == PLAYER_LIMB_L_THIGH || limb == PLAYER_LIMB_R_THIGH)
        rot.z -= angle(jump.bend);
    if (limb == PLAYER_LIMB_L_SHIN || limb == PLAYER_LIMB_R_SHIN)
        rot.z += angle(jump.bend * 1.8f);
    if (limb == PLAYER_LIMB_UPPER)
        rot.z += angle(jump.torso);
    if (limb == PLAYER_LIMB_L_SHOULDER)
        rot.z += angle(jump.freeArm);
    if (limb == PLAYER_LIMB_L_FOREARM)
        rot.z -= angle(.20f * jump.tuck);
    if (!carry) {
        if (limb == PLAYER_LIMB_R_SHOULDER)
            rot.z -= angle(.24f * jump.tuck);
        if (limb == PLAYER_LIMB_R_FOREARM)
            rot.z += angle(.20f * jump.tuck);
    } else if (limb == PLAYER_LIMB_R_SHOULDER)
        rot.x += angle(jump.carryArm);
}
} // namespace NativeSkateJumpRender

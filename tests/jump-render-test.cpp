#include <cassert>
#include <cstdio>
#include <cstdint>
struct Vec3f {
    float x = 0, y = 0, z = 0;
};
struct Vec3s {
    int16_t x = 0, y = 0, z = 0;
};
enum {
    PLAYER_LIMB_ROOT = 1,
    PLAYER_LIMB_WAIST = 2,
    PLAYER_LIMB_L_THIGH = 7,
    PLAYER_LIMB_L_SHIN = 8,
    PLAYER_LIMB_R_THIGH = 4,
    PLAYER_LIMB_R_SHIN = 5,
    PLAYER_LIMB_UPPER = 10,
    PLAYER_LIMB_L_SHOULDER = 14,
    PLAYER_LIMB_L_FOREARM = 15,
    PLAYER_LIMB_R_SHOULDER = 17,
    PLAYER_LIMB_R_FOREARM = 18
};
#include "NativeSkateJumpRender.h"
int main() {
    for (bool carry : { false, true })
        for (float t : { .08f, .45f, .9f }) {
            auto p = NativeSkatePresentationMath::JumpPose(true, t, 0, carry);
            for (int limb : { PLAYER_LIMB_WAIST, PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN, PLAYER_LIMB_R_THIGH,
                              PLAYER_LIMB_R_SHIN, PLAYER_LIMB_L_SHOULDER, PLAYER_LIMB_L_FOREARM }) {
                Vec3f pos{};
                Vec3s rot{};
                NativeSkateJumpRender::Apply(limb, pos, rot, p, carry);
                assert(rot.z != 0);
            }
            auto base = NativeSkatePresentationMath::JumpBase(true, t, 0, 0);
            assert(base.kind == 1 && base.progress >= 0 && base.progress <= 1);
        }
    assert(NativeSkatePresentationMath::JumpBase(false, 0, .1f, 0).kind == 3);
    puts("PASS asset-free final limb helper + stock jump/landing base; human render acceptance is separately retained");
}

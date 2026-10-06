#include "NativeSkatePresentationMath.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace NativeSkatePresentationMath;
    for (int hz : { 60, 120, 144, 240 }) {
        for (int age : { 0, 1 }) {
            (void)age;
            for (int f = 0; f < hz * 2; ++f) {
                float t = float(f) / hz;
                auto a = JumpPose(true, t, 0, false), b = JumpPose(true, t, 0, true);
                assert(std::isfinite(a.bend) && a.bend >= 0 && a.bend < .5f);
                if (t > .1f && t < .55f) {
                    assert(b.freeArm > a.freeArm && b.torso < a.torso && b.carryArm > 0 && b.carryArm < .20f);
                }
                auto land = JumpPose(false, 0, t, true);
                assert(std::isfinite(land.compression) && land.compression >= -1e-6f && land.compression <= 1);
            }
        }
    }
    for (float speed : { 0.f, 1.f, 4.f })
        assert(BipedJumpAnimation(2, speed, true) == 0);
    assert(BipedJumpAnimation(0, 4, false) == 2);
    assert(JumpVoice(500, 501, 2));
    assert(!JumpVoice(501, 501, 2));
    assert(!JumpVoice(500, 501, -1));
    assert(!JumpVoice(100, 200, 2));
    assert(!JumpVoice(500, 500, 0));
    puts(
        "PASS distinct empty/carry jump curves, bounded secondary carry flex, landing recovery, one successful Biped takeoff voice edge and no ollie/held/rejected/ledge-fall voice at60/120/144/240");
}

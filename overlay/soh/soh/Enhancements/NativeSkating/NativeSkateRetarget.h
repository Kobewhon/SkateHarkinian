#pragma once
#include "NativeSkatePose.h"
#include "NativeSkateLinkFrame.h"
#include "NativeSkateUnits.h"
extern "C" {
#include "z64.h"
}
namespace NativeSkateRetarget {
struct BodyInfo {
    uint64_t poseTick = 0;
    float joints[PLAYER_LIMB_MAX][3] = {};
    bool valid[PLAYER_LIMB_MAX] = {};
    float pelvisFrame[16] = {};
    float nativeBodyFrame[16] = {}, linkBodyFrame[16] = {};
    float heldRelativeScale = 1;
    float localRoot[3] = {};
    float handplantOffset[3] = {};
    float up[PLAYER_LIMB_MAX][3] = {};
    float leftSupport = .5f;
    bool whole = false, ragdoll = false;
};
struct ContactInfo {
    uint64_t tick = 0;
    float visualScale = 1;
    float nativeFeet[2][16] = {}, nativePelvis[16] = {}, linkFeet[2][16] = {}, linkPelvis[16] = {}, linkRoot[16] = {},
          nativeLocal[2][3] = {}, linkLocal[2][3] = {}, linkSoles[2][3] = {}, targets[2][3] = {}, clearance[2] = {},
          nativeClearance[2] = {}, error[2] = {}, weight[2] = {}, board[16] = {};
    bool valid = false;
};
const ContactInfo& ContactDiagnostics();
bool CarryArm(Player*, Vec3s*, const Vec3s*, const NativeSkateRuntime::SkaterPose*, float);
void HandplantHand(Player*, const NativeSkateLink::Frame&, Vec3s*, const NativeSkateRuntime::SkaterPose*,
                   const float target[3], float weight = 1.f);
void GrabHands(Player*, const NativeSkateLink::Frame&, Vec3s*, const NativeSkateRuntime::SkaterPose*,
               const NativeSkateUnits::Anchor&, const float*);
void PropArms(Player*, Vec3s*, const Vec3s*, const NativeSkateRuntime::SkaterPose*);
void Contact(Player*, const NativeSkateLink::Frame&, Vec3s*, const NativeSkateRuntime::SkaterPose*,
             const NativeSkateUnits::Anchor&, const float*, const float*);
const BodyInfo& BodyDiagnostics();
void Reset();
void FrameStart();
bool Apply(Player*, const NativeSkateLink::Frame&, Vec3s*, const NativeSkateRuntime::SkaterPose*,
           const NativeSkateUnits::Anchor&, const float* diagnostics, const float* board, float pitch);
bool HeldBoard(const NativeSkateRuntime::SkaterPose*, const NativeSkateUnits::Anchor&, const float* diagnostics,
               float output[16]);
void Capture(s32 limb);
void Debug(PlayState*, const NativeSkateRuntime::SkaterPose*, const NativeSkateUnits::Anchor&, bool active,
           const char* state, const char* error);
} // namespace NativeSkateRetarget

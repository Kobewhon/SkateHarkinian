#pragma once
#include "NativeSkateLinkFrame.h"
#include "NativeSkatePose.h"
#include "NativeSkateUnits.h"
struct PlayState;
struct Player;
namespace NativeSkateLink {
void Begin(PlayState* play, Player* player);
void End();
void Sword(bool active);
void UpdateVisual(PlayState*, Player*, const char* state, float speed, uint64_t tick);
float HeadY();
bool CarryMatrix(const Frame&, float spatialScale, float out[16]);
const char* AnimationName();
void TraceNativePose(Player*, const Frame&, const NativeSkateRuntime::SkaterPose*, const NativeSkateUnits::Anchor&,
                     const float*, const float*, float);
void Draw(PlayState* play, Player* player, const Frame& frame, const NativeSkateRuntime::SkaterPose*,
          const NativeSkateUnits::Anchor&, const char* state, const float* riderDiagnostics, const float* board,
          float bodyPitch);
void Debug(PlayState* play, Player* player, const Frame& frame, const float nativeRoot[3], bool proxy,
           const float* board, bool carry, float spatialScale);
} // namespace NativeSkateLink

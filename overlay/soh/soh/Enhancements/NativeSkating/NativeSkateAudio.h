#pragma once
#include "NativeSkateRuntime.h"
struct PlayState;
struct Player;
namespace NativeSkateAudio {
void Update(PlayState*, Player*, const NativeSkateRuntime::Snapshot&, bool push, bool water, bool held);
void Stop();
void Mix(int16_t* samples, uint32_t stereoFrames);
} // namespace NativeSkateAudio

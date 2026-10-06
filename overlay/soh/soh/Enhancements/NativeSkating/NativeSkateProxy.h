#pragma once
#include "NativeSkateRuntime.h"
struct PlayState;
namespace NativeSkateProxy {
void Draw(PlayState* play, const NativeSkateRuntime::Snapshot& state, const float riderPosition[3],
          const float boardPosition[3], bool debug, uint32_t collisionCount, uint32_t retained, const char* error,
          float spatialScale, const float* dimensions, const float* biped, const char* previous, float moveDot,
          bool moveDotValid, bool drawRider, const float* visualBoard = nullptr, bool drawBoard = true);
void DrawStatus(PlayState* play, const char* status, const char* error);
void DrawPushStatus(PlayState* play, uint64_t presses, uint64_t events, uint64_t applications, float delta, float dt,
                    double ticks, double samples, double hostFPS, bool xDown, const char* stop);
void DrawModuleStatus(PlayState* play, uint64_t updateTicks, uint64_t drawTicks, bool requested, bool enabled,
                      bool f8Down, bool f8Pressed, bool ownership, const char* status, const char* error);
} // namespace NativeSkateProxy

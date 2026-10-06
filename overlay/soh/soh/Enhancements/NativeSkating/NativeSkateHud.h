#pragma once
#include "NativeSkateHudState.h"
struct PlayState;
namespace NativeSkateHud {
void InitializeFont();
bool FontReady();
void ClearOverlays();
void Overlay(const char*, float, float, float, int, int, int, int, bool right = false);
void Draw(PlayState*, const State&);
} // namespace NativeSkateHud

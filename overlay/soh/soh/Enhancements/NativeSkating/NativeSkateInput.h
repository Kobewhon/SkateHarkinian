#pragma once
#include "NativeSkateRuntime.h"
#include <deque>
#include <functional>
#include <vector>
namespace NativeSkateInput {
void Initialize();
void Reset(double now);
void Poll();
struct DropperTriggerState {
    int leftRaw = 0, rightRaw = 0, thresholdPercent = 25;
    bool leftHeld = false, rightHeld = false;
};
DropperTriggerState ReadDropperTriggers();
// Freeze/rebase the input clock without resetting native ticks or session.
void Pause(double now);
void Resume(double now);
double Now();
bool AdvanceTo(double now, const std::function<bool(const std::vector<NativeSkateRuntime::Packet>&)>& step);
NativeSkateRuntime::Packet Current();
uint32_t Retained();
struct Diagnostics {
    uint64_t presses = 0, samples = 0;
    bool xDown = false;
    int16_t r2Raw = 0;
    double elapsed = 0;
};
Diagnostics GetDiagnostics();
} // namespace NativeSkateInput

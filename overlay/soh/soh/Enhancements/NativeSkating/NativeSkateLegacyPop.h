#pragma once
#include <cmath>
#include <cstdint>
namespace NativeSkateLegacyPop {
struct State {
    bool eligible = false, triggerSeen = false, consumed = false, previousTriangle = false, previousTrick = false,
         primed = false;
    uint8_t previousTrigger = 0;
    uint64_t generation = 0, scene = 0;
    double triggerTime = 0, startTime = 0;
    void Reset() {
        *this = {};
    }
    bool Read(bool enabled, uint64_t sceneGeneration, bool authoredFs360, bool airborne, uint8_t trigger, bool triangle,
              bool validLanding, double touchdownSeconds, double time) {
        if (!std::isfinite(time) || scene != sceneGeneration) {
            Reset();
            scene = sceneGeneration;
        }
        if (!primed) {
            primed = true;
            previousTrigger = trigger;
            previousTriangle = triangle;
            previousTrick = authoredFs360;
            return false;
        }
        const bool triggerEdge = (trigger & ~previousTrigger) != 0, triangleEdge = triangle && !previousTriangle;
        previousTrigger = trigger;
        previousTriangle = triangle;
        if (authoredFs360 && !previousTrick) {
            ++generation;
            eligible = true;
            triggerSeen = false;
            consumed = false;
            startTime = time;
        }
        previousTrick = authoredFs360;
        if (!enabled || !airborne) {
            eligible = false;
            triggerSeen = false;
            return false;
        }
        if (!eligible || consumed)
            return false;
        if (triggerEdge && time > startTime) {
            triggerSeen = true;
            triggerTime = time;
        }
        if (triangleEdge && triggerSeen && time > triggerTime && validLanding && std::isfinite(touchdownSeconds) &&
            touchdownSeconds >= 0 && touchdownSeconds <= .16) {
            consumed = true;
            eligible = false;
            return true;
        }
        return false;
    }
};
} // namespace NativeSkateLegacyPop

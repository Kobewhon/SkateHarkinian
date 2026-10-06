#include "NativeSkateInput.h"
#include <SDL2/SDL.h>
#include <ship/Context.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>

namespace NativeSkateInput {
namespace {
struct Timed {
    double time;
    NativeSkateRuntime::Packet packet;
};
std::mutex gMutex;
std::deque<Timed> gSamples;
NativeSkateRuntime::Packet gCurrent{};
int gSelected = -1;
double gStart = 0.0, gLast = -1.0;
uint64_t gTicks = 0;
NativeSkateRuntime::Packet gHeld{};
bool gWatchInstalled = false;
bool gPaused = false;
double gPauseStart = 0;
Diagnostics gDiagnostics;
uint16_t gRecordedButtons = 0;
float Axis(Sint16 v) {
    return v < 0 ? float(v) / 32768.0f : float(v) / 32767.0f;
}
int16_t ToI16(float v) {
    v = std::max(-1.0f, std::min(1.0f, v));
    return (int16_t)std::lrintf(v * (v < 0 ? 32768.0f : 32767.0f));
}
uint16_t ButtonMask(uint8_t b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return 0x0001;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return 0x0002;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return 0x0004;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return 0x0008;
        case SDL_CONTROLLER_BUTTON_START:
            return 0x0010;
        case SDL_CONTROLLER_BUTTON_BACK:
            return 0x0020;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            return 0x0040;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            return 0x0080;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return 0x0100;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return 0x0200;
        case SDL_CONTROLLER_BUTTON_A:
            return 0x1000;
        case SDL_CONTROLLER_BUTTON_B:
            return 0x2000;
        case SDL_CONTROLLER_BUTTON_X:
            return 0x4000;
        case SDL_CONTROLLER_BUTTON_Y:
            return 0x8000;
        default:
            return 0;
    }
}
void Record(double t) {
    if (gStart == 0.0 || gPaused)
        return;
    t = std::max(t, gStart + double(gTicks) / 60.0);
    if ((gCurrent.buttons & 0x1000) && !(gRecordedButtons & 0x1000))
        ++gDiagnostics.presses;
    const bool buttonChanged = gRecordedButtons != gCurrent.buttons;
    gRecordedButtons = gCurrent.buttons;
    gDiagnostics.xDown = (gCurrent.buttons & 0x1000) != 0;
    ++gDiagnostics.samples;
    gDiagnostics.elapsed = std::max(0.0, t - gStart);
    // Never overwrite a pressed/released edge with another event in the same millisecond.
    if (t == gLast && !gSamples.empty() && !buttonChanged && gSamples.back().packet.buttons == gCurrent.buttons)
        gSamples.back().packet = gCurrent;
    else {
        gSamples.push_back({ t, gCurrent });
        gLast = t;
        if (gSamples.size() > 512)
            gSamples.pop_front();
    }
}
int SDLCALL Watch(void*, SDL_Event* e) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (e->type == SDL_CONTROLLERAXISMOTION && e->caxis.which == gSelected) {
        switch (e->caxis.axis) {
            case SDL_CONTROLLER_AXIS_LEFTX:
                gCurrent.left[0] = ToI16(Axis(e->caxis.value));
                break;
            case SDL_CONTROLLER_AXIS_LEFTY:
                gCurrent.left[1] = ToI16(-Axis(e->caxis.value));
                break;
            case SDL_CONTROLLER_AXIS_RIGHTX:
                gCurrent.right[0] = ToI16(Axis(e->caxis.value));
                break;
            case SDL_CONTROLLER_AXIS_RIGHTY:
                gCurrent.right[1] = ToI16(-Axis(e->caxis.value));
                break;
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                gCurrent.triggers[0] = (uint8_t)std::lrintf(std::max(0.0f, Axis(e->caxis.value)) * 255.0f);
                break;
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                gDiagnostics.r2Raw = e->caxis.value;
                gCurrent.triggers[1] = (uint8_t)std::lrintf(std::max(0.0f, Axis(e->caxis.value)) * 255.0f);
                break;
            default:
                return 1;
        }
        Uint64 now = SDL_GetTicks64();
        Sint32 age = (Sint32)((Uint32)now - e->caxis.timestamp);
        Record((now - (Uint64)std::max(0, age)) / 1000.0);
        return 1;
    }
    if ((e->type == SDL_CONTROLLERBUTTONDOWN || e->type == SDL_CONTROLLERBUTTONUP) && e->cbutton.which == gSelected) {
        uint16_t mask = ButtonMask(e->cbutton.button);
        if (!mask)
            return 1;
        if (e->type == SDL_CONTROLLERBUTTONDOWN)
            gCurrent.buttons |= mask;
        else
            gCurrent.buttons &= (uint16_t)~mask;
        Record(SDL_GetTicks64() / 1000.0);
        return 1;
    }
    return 1;
}
} // namespace
void Initialize() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gWatchInstalled) {
        SDL_AddEventWatch(Watch, nullptr);
        gWatchInstalled = true;
    }
}
double Now() {
    return SDL_GetTicks64() / 1000.0;
}
void Reset(double now) {
    std::lock_guard<std::mutex> lock(gMutex);
    gSamples.clear();
    gCurrent = {};
    gHeld = {};
    gStart = now;
    gLast = -1.0;
    gTicks = 0;
    gDiagnostics = {};
    gRecordedButtons = 0;
    gPaused = false;
    gPauseStart = 0;
    Record(now);
}
void Pause(double now) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gPaused) {
        gPaused = true;
        gPauseStart = now;
        gSamples.clear();
    }
}
void Resume(double now) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gPaused) {
        gStart += std::max(0.0, now - gPauseStart);
        gPaused = false;
        gLast = -1;
        gSamples.clear();
        gHeld = gCurrent;
        gRecordedButtons = gCurrent.buttons;
        Record(now);
    }
}
void Poll() {
    auto* context = Ship::Context::GetRawInstance();
    if (!context)
        return;
    auto deck = context->GetControlDeck();
    if (!deck)
        return;
    auto devices = deck->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(0);
    std::lock_guard<std::mutex> lock(gMutex);
    if (!devices.count(gSelected)) {
        gSelected = -1;
        for (const auto& device : devices)
            if (gSelected < 0 || device.first < gSelected)
                gSelected = device.first;
        gCurrent = {};
    }
    if (gSelected < 0 || !devices.count(gSelected))
        return;
    SDL_GameController* c = devices.at(gSelected);
    const uint8_t sdlButtons[] = { SDL_CONTROLLER_BUTTON_A,
                                   SDL_CONTROLLER_BUTTON_B,
                                   SDL_CONTROLLER_BUTTON_X,
                                   SDL_CONTROLLER_BUTTON_Y,
                                   SDL_CONTROLLER_BUTTON_BACK,
                                   SDL_CONTROLLER_BUTTON_START,
                                   SDL_CONTROLLER_BUTTON_LEFTSTICK,
                                   SDL_CONTROLLER_BUTTON_RIGHTSTICK,
                                   SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
                                   SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                                   SDL_CONTROLLER_BUTTON_DPAD_UP,
                                   SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                   SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                                   SDL_CONTROLLER_BUTTON_DPAD_RIGHT };
    gCurrent.buttons = 0;
    for (uint8_t b : sdlButtons)
        if (SDL_GameControllerGetButton(c, (SDL_GameControllerButton)b))
            gCurrent.buttons |= ButtonMask(b);
    gCurrent.left[0] = ToI16(Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX)));
    gCurrent.left[1] = ToI16(-Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY)));
    gCurrent.right[0] = ToI16(Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX)));
    gCurrent.right[1] = ToI16(-Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY)));
    gCurrent.triggers[0] = (uint8_t)std::lrintf(
        std::max(0.0f, Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT))) * 255.0f);
    gDiagnostics.r2Raw = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    gCurrent.triggers[1] = (uint8_t)std::lrintf(
        std::max(0.0f, Axis(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT))) * 255.0f);
    Record(Now());
}
DropperTriggerState ReadDropperTriggers() {
    DropperTriggerState result;
    auto* context = Ship::Context::GetRawInstance();
    if (!context)
        return result;
    auto deck = context->GetControlDeck();
    if (!deck)
        return result;
    result.thresholdPercent = deck->GetGlobalSDLDeviceSettings()->GetTriggerAxisThresholdPercentage();
    // Same live SDL source and digital-held threshold as SDLAxisDirectionToButtonMapping::UpdatePad.
    for (const auto& device : deck->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(0)) {
        result.leftRaw =
            std::max(result.leftRaw, (int)SDL_GameControllerGetAxis(device.second, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
        result.rightRaw =
            std::max(result.rightRaw, (int)SDL_GameControllerGetAxis(device.second, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    }
    float threshold = SDL_JOYSTICK_AXIS_MAX * (result.thresholdPercent / 100.f);
    result.leftHeld = result.leftRaw > threshold;
    result.rightHeld = result.rightRaw > threshold;
    return result;
}
NativeSkateRuntime::Packet Current() {
    std::lock_guard<std::mutex> lock(gMutex);
    return gCurrent;
}
uint32_t Retained() {
    std::lock_guard<std::mutex> lock(gMutex);
    return (uint32_t)gSamples.size();
}
Diagnostics GetDiagnostics() {
    std::lock_guard<std::mutex> lock(gMutex);
    return gDiagnostics;
}
bool AdvanceTo(double now, const std::function<bool(const std::vector<NativeSkateRuntime::Packet>&)>& step) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gPaused)
        return true;
    if (!gStart || !std::isfinite(now) || now < gStart)
        return false;
    // Mutex-protected reusable storage; event queue is bounded at 512.
    static std::vector<NativeSkateRuntime::Packet> batch;
    if (batch.capacity() < 512)
        batch.reserve(512);
    while ((gTicks + 1) / 60.0 <= now - gStart + 1e-9) {
        double boundary = gStart + double(gTicks + 1) / 60.0;
        batch.clear();
        while (!gSamples.empty() && gSamples.front().time <= boundary + 1e-9) {
            gHeld = gSamples.front().packet;
            batch.push_back(gHeld);
            gSamples.pop_front();
        }
        if (batch.empty())
            batch.push_back(gHeld);
        if (!step(batch))
            return false;
        ++gTicks;
    }
    return true;
}
} // namespace NativeSkateInput

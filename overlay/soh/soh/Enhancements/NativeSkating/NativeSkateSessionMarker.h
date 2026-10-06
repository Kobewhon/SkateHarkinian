#pragma once
#include "NativeSkateRuntime.h"
#include "NativeSkateMarkerCamera.h"
namespace NativeSkateSessionMarker {
struct Saved {
    bool valid = false;
    int scene = -1, room = -1;
    NativeSkateRuntime::Marker native{};
    CameraPose camera{};
};
} // namespace NativeSkateSessionMarker

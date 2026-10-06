#pragma once
#include "NativeSkateGeometry.h"
#include "NativeSkateSceneProfile.h"
#include "../NativeSkateWorld.h"
#include "../NativeSkateRuntime.h"
#include "../NativeSkateUnits.h"
namespace NativeSkateWorldDebug {
struct Contact {
    float ground = 0, deckBottom = 0, visualBottom = 0, clearance = 0;
    bool floorValid = false;
    NativeSkateGeometry::Point normal{};
};
Contact Measure(const NativeSkateGeometry::Result&, const NativeSkateRuntime::Snapshot&,
                const NativeSkateUnits::Anchor&, const float*);
void Draw(PlayState*, const NativeSkateGeometry::Result&, const NativeSkateWorld::Snapshot&,
          const NativeSkateSceneProfile::Profile&, const NativeSkateRuntime::Snapshot&, const NativeSkateUnits::Anchor&,
          float, const float*, bool riderDebug = false);
} // namespace NativeSkateWorldDebug

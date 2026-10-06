#pragma once
#include "NativeSkateGeometry.h"
#include <string>
namespace NativeSkateSceneProfile {
struct Profile {
    int sceneId = -1;
    bool collision = true, stairSmoothing = true, spatialExperiment = false;
    float microTolerance = .025f, worldScale = 1;
    NativeSkateGeometry::Point anchor{};
    struct Addition {
        std::string type, material;
        NativeSkateGeometry::Point position{}, dimensions{};
        float yaw = 0, rise = 0;
        bool enabled = false;
    };
    std::vector<Addition> additions;
    std::string error;
};
void Invalidate();
Profile Load(int sceneId, const std::string& directory);
void Apply(const Profile& profile, NativeSkateGeometry::Result& result);
} // namespace NativeSkateSceneProfile

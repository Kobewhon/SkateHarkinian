#include "NativeSkateSceneProfile.h"
#include "../NativeSkateUnits.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <optional>
#include <cmath>
#include <algorithm>
namespace NativeSkateSceneProfile {
static std::optional<nlohmann::json> cached;
static std::string directoryKey;
static bool loaded = false;
static std::string loadError;
void Invalidate() {
    cached.reset();
    loaded = false;
    loadError.clear();
}
Profile Load(int sceneId, const std::string& directory) {
    Profile p;
    p.sceneId = sceneId;
    p.spatialExperiment = sceneId == 0x51;
    if (!loaded || directoryKey != directory) {
        Invalidate();
        directoryKey = directory;
        loaded = true;
        std::ifstream file(directory + "/native-skate-world.json");
        if (file)
            try {
                nlohmann::json j;
                file >> j;
                cached = std::move(j);
            } catch (const std::exception& e) {
                loadError = e.what();
            }
    }
    if (!loadError.empty()) {
        p.collision = false;
        p.error = loadError;
        return p;
    }
    if (!cached)
        return p;
    try {
        const auto& data = *cached;
        if (data.at("version").get<int>() != 1)
            throw std::runtime_error("Unsupported profile version");
        for (const auto& j : data.at("scenes")) {
            if (j.at("sceneId").get<int>() != sceneId)
                continue;
            p.collision = j.value("skateCollisionEnabled", true);
            p.stairSmoothing = j.value("stairSmoothing", true);
            p.microTolerance = j.value("microEdgeToleranceMeters", .025f);
            p.worldScale = j.value("worldScale", 1.f);
            p.anchor = j.value("scaleAnchorHost", NativeSkateGeometry::Point{});
            p.spatialExperiment = sceneId == 0x51 && j.value("nativeSpatialExperiment", true);
            if (!std::isfinite(p.microTolerance) || p.microTolerance < 0 || p.microTolerance > .06f ||
                !std::isfinite(p.worldScale) || p.worldScale < 1 || p.worldScale > 1.33f)
                throw std::runtime_error("Profile tolerance/scale outside safe range");
            for (float a : p.anchor)
                if (!std::isfinite(a) || std::abs(a) > 30000)
                    throw std::runtime_error("Invalid scale anchor");
            for (auto& s : j.value("customSurfaces", nlohmann::json::array())) {
                Profile::Addition a;
                a.type = s.at("type");
                a.enabled = s.value("enabled", false);
                a.position = s.at("positionHost").get<NativeSkateGeometry::Point>();
                a.dimensions = s.at("dimensionsHost").get<NativeSkateGeometry::Point>();
                a.yaw = s.value("yawDegrees", 0.f) * .01745329252f;
                a.rise = s.value("riseHost", 0.f);
                a.material = s.value("material", std::string("native-default"));
                if (a.type != "PLANE" && a.type != "RAMP" && a.type != "BANK" && a.type != "WEDGE")
                    throw std::runtime_error("Unsupported custom surface type");
                if (a.material != "native-default")
                    throw std::runtime_error("Custom material unavailable in triangle ABI");
                for (float f : a.position)
                    if (!std::isfinite(f) || std::abs(f) > 30000)
                        throw std::runtime_error("Invalid surface position");
                for (float f : a.dimensions)
                    if (!std::isfinite(f) || f < 0 || f > 2000)
                        throw std::runtime_error("Invalid surface dimensions");
                if (a.dimensions[0] < 1 || a.dimensions[2] < 1 || !std::isfinite(a.rise) || a.rise < 0 ||
                    a.rise > 100 || !std::isfinite(a.yaw))
                    throw std::runtime_error("Invalid custom surface rise/yaw");
                p.additions.push_back(a);
            }
            break;
        }
    } catch (const std::exception& e) {
        p = Profile{};
        p.sceneId = sceneId;
        p.collision = false;
        p.spatialExperiment = false;
        p.error = e.what();
    }
    return p;
}
void Apply(const Profile& p, NativeSkateGeometry::Result& r) {
    for (const auto& a : p.additions) {
        if (!a.enabled)
            continue;
        auto position = a.position, dim = a.dimensions;
        for (float& v : position)
            v /= NativeSkateUnits::kOotUnitsPerMeter;
        for (float& v : dim)
            v /= NativeSkateUnits::kOotUnitsPerMeter;
        float rise = a.type == "PLANE" ? 0 : a.rise / NativeSkateUnits::kOotUnitsPerMeter;
        size_t start = r.triangles.size();
        NativeSkateGeometry::Plane(r, position, a.yaw, dim, rise,
                                   a.type == "PLANE" ? NativeSkateGeometry::Surface::Flat
                                                     : NativeSkateGeometry::Surface::Bank);
        if (a.type == "WEDGE" && r.triangles.size() == start + 2) {
            auto top = r.triangles[start];
            auto left = r.triangles[start + 1];
            auto A = top.a, B = top.c, C = top.b, D = left.b, cb = C, db = D;
            cb[1] -= rise;
            db[1] -= rise;
            r.triangles.push_back({ B, C, cb, NativeSkateGeometry::Surface::Wall, true });
            r.triangles.push_back({ A, db, D, NativeSkateGeometry::Surface::Wall, true });
            r.triangles.push_back({ C, D, db, NativeSkateGeometry::Surface::Wall, true });
            r.triangles.push_back({ C, db, cb, NativeSkateGeometry::Surface::Wall, true });
            r.triangles.push_back({ A, B, cb, NativeSkateGeometry::Surface::TooSteep, true });
            r.triangles.push_back({ A, cb, db, NativeSkateGeometry::Surface::TooSteep, true });
        }
        for (size_t i = start; i < r.triangles.size(); ++i)
            r.triangles[i].visible = true;
        if (a.type == "BANK")
            ++r.banks;
        else if (a.type != "PLANE")
            ++r.ramps;
    }
}
} // namespace NativeSkateSceneProfile

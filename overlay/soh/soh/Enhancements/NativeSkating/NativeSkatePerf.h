#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <algorithm>
namespace NativeSkatePerf {
enum Stage {
    Prewarm,
    StaticPrep,
    Extract,
    Classify,
    Weld,
    Upload,
    DynaDiscovery,
    DynaMesh,
    DynaTransform,
    DynaCompile,
    DynaGrindTransform,
    EventDrain,
    HudUpdate,
    HudDraw,
    SceneReseed,
    Activation,
    Count
};
inline const char* Name(Stage s) {
    const char* names[] = { "PREWARM",        "STATIC_PREP",       "EDGE_EXTRACT",         "CLASSIFY",
                            "WELD",           "GRIND_UPLOAD",      "DYNA_DISCOVERY",       "DYNA_LOCAL_MESH",
                            "DYNA_TRANSFORM", "DYNA_LOCAL_GRINDS", "DYNA_GRIND_TRANSFORM", "EVENT_DRAIN",
                            "HUD_UPDATE",     "HUD_DRAW",          "SCENE_RESEED",         "READY_TO_ACTIVE" };
    return names[s];
}
struct Metric {
    uint64_t count = 0;
    double total = 0, max = 0;
    void Add(double ms) {
        ++count;
        total += ms;
        max = std::max(max, ms);
    }
    double Average() const {
        return count ? total / count : 0;
    }
};
inline std::array<Metric, Count> metrics{}; // Only gameplay thread publishes these metrics.
inline bool enabled = false;
struct Scope {
    Stage stage;
    bool active;
    std::chrono::steady_clock::time_point start{};
    explicit Scope(Stage s) : stage(s), active(enabled) {
        if (active)
            start = std::chrono::steady_clock::now();
    }
    ~Scope() {
        if (active)
            metrics[stage].Add(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
};
} // namespace NativeSkatePerf

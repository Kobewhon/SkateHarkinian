#pragma once
#include <array>
#include <optional>
#include <vector>
#include <cmath>
#include <cstdint>
namespace NativeSkateObjects {
using Id = uint64_t;
constexpr size_t kMaxObjects = 64;
constexpr size_t kTypeCount = 53;
enum class Type {
    Marker,
    Box,
    Vhs,
    Ramp,
    Rail,
    FlatLow,
    FlatTall,
    FlatLong,
    RoundLow,
    RoundMedium,
    DownShort,
    DownMedium,
    Kink,
    Handrail,
    KickerSmall,
    KickerMedium,
    KickerLarge,
    BankSmall,
    BankMedium,
    WedgeLarge,
    QuarterSmall,
    QuarterMedium,
    Funbox,
    QuarterTall,
    QuarterWide,
    MiniLeft,
    MiniRight,
    BankTall,
    BankWide,
    BankLedge,
    Pyramid,
    HipLeft,
    HipRight,
    Spine,
    LaunchLong,
    ManualLow,
    StairsSmall,
    FlatExtraLow,
    FlatExtraLong,
    RoundLong,
    RoundTall,
    DownLong,
    DownSteep,
    UpRail,
    Rainbow,
    DoubleKink,
    AFrame,
    FlatDown,
    DownFlat,
    PhysicsKickerSmall,
    PhysicsKickerMedium,
    PhysicsBankSmall,
    PhysicsManualLow
};
enum class Collision { None, StaticSolid };
enum class Lifetime { Transient, Scene, Session, Persistent };
enum class Category { Ramps, Rails, Ledges, Props, Special, Debug, Placed };
enum class Behavior { VisualOnly, Static, Movable, SkateSurface, Grindable };
enum class AudioSurface { Generic, Stone, Concrete, Wood, Metal, Dirt, Grass };
enum class Profile {
    Box,
    Flat,
    Round,
    Down,
    Kink,
    Wedge,
    Kicker,
    Bank,
    Quarter,
    Funbox,
    Mini,
    MiniReverse,
    BankLedge,
    Pyramid,
    HipLeft,
    HipRight,
    Spine,
    Pad,
    Stairs,
    Up,
    Rainbow,
    DoubleKink,
    AFrame,
    DownFlat
};
struct Definition {
    const char* id;
    const char* name;
    Collision collision;
    Lifetime lifetime;
    float width, height, length, scale;
    const char* resource;
    float visualYOffset = 0;
    uint8_t allowedRotationAxes = 7;
    Category category = Category::Debug;
    Behavior behavior = Behavior::VisualOnly;
    AudioSurface audio = AudioSurface::Generic;
    Profile profile = Profile::Box;
};
inline const std::array<Definition, kTypeCount>& Definitions() {
    static const std::array<Definition, kTypeCount> d = {
        { { "SKATE_DEBUG_MARKER", "Debug Marker", Collision::None, Lifetime::Transient, 12, 24, 12, 1, nullptr, 0, 7,
            Category::Debug, Behavior::VisualOnly },
          { "SKATE_DEBUG_BOX", "Debug Box", Collision::StaticSolid, Lifetime::Transient, 50, 30, 50, 1, nullptr, 0, 7,
            Category::Debug, Behavior::Movable, AudioSurface::Wood },
          { "SKATE_VHS_PICKUP_TEST", "VHS Tape", Collision::None, Lifetime::Transient, 12, 8, 3, .01f,
            "SkateHarkinian/VHS/Mesh/DisplayList", 5, 7, Category::Special, Behavior::VisualOnly },
          { "Ramp", "Wedge - Small", Collision::StaticSolid, Lifetime::Transient, 80, 50, 180, 1,
            "SkateHarkinian/Props/Ramp/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface, AudioSurface::Wood,
            Profile::Wedge },
          { "Rail", "Flat Rail - Medium", Collision::StaticSolid, Lifetime::Transient, 12, 32, 200, 1,
            "SkateHarkinian/Props/Rail/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::Flat },
          { "FlatLow", "Flat Rail - Low", Collision::StaticSolid, Lifetime::Transient, 12, 20, 180, 1,
            "SkateHarkinian/Props/FlatLow/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::Flat },
          { "FlatTall", "Flat Rail - Tall", Collision::StaticSolid, Lifetime::Transient, 12, 52, 220, 1,
            "SkateHarkinian/Props/FlatTall/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Flat },
          { "FlatLong", "Flat Rail - Long", Collision::StaticSolid, Lifetime::Transient, 12, 32, 360, 1,
            "SkateHarkinian/Props/FlatLong/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Flat },
          { "RoundLow", "Round Rail - Low", Collision::StaticSolid, Lifetime::Transient, 10, 20, 180, 1,
            "SkateHarkinian/Props/RoundLow/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Round },
          { "RoundMedium", "Round Rail - Medium", Collision::StaticSolid, Lifetime::Transient, 10, 36, 240, 1,
            "SkateHarkinian/Props/RoundMedium/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Round },
          { "DownShort", "Down Rail - Short", Collision::StaticSolid, Lifetime::Transient, 12, 60, 200, 1,
            "SkateHarkinian/Props/DownShort/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Down },
          { "DownMedium", "Down Rail - Medium", Collision::StaticSolid, Lifetime::Transient, 12, 90, 320, 1,
            "SkateHarkinian/Props/DownMedium/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Down },
          { "Kink", "Kink Rail", Collision::StaticSolid, Lifetime::Transient, 12, 75, 320, 1,
            "SkateHarkinian/Props/Kink/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::Kink },
          { "Handrail", "Basic Handrail", Collision::StaticSolid, Lifetime::Transient, 10, 90, 300, 1,
            "SkateHarkinian/Props/Handrail/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Down },
          { "KickerSmall", "Kicker - Small", Collision::StaticSolid, Lifetime::Transient, 90, 25, 100, 1,
            "SkateHarkinian/Props/KickerSmall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Kicker },
          { "KickerMedium", "Kicker - Medium", Collision::StaticSolid, Lifetime::Transient, 120, 45, 150, 1,
            "SkateHarkinian/Props/KickerMedium/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Kicker },
          { "KickerLarge", "Kicker - Large", Collision::StaticSolid, Lifetime::Transient, 150, 65, 200, 1,
            "SkateHarkinian/Props/KickerLarge/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Kicker },
          { "BankSmall", "Bank - Small", Collision::StaticSolid, Lifetime::Transient, 140, 40, 160, 1,
            "SkateHarkinian/Props/BankSmall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Bank },
          { "BankMedium", "Bank - Medium", Collision::StaticSolid, Lifetime::Transient, 200, 70, 240, 1,
            "SkateHarkinian/Props/BankMedium/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Bank },
          { "WedgeLarge", "Wedge - Large", Collision::StaticSolid, Lifetime::Transient, 160, 80, 240, 1,
            "SkateHarkinian/Props/WedgeLarge/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Wedge },
          { "QuarterSmall", "Quarter Pipe - Small", Collision::StaticSolid, Lifetime::Transient, 160, 90, 150, 1,
            "SkateHarkinian/Props/QuarterSmall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Quarter },
          { "QuarterMedium", "Quarter Pipe - Medium", Collision::StaticSolid, Lifetime::Transient, 220, 130, 210, 1,
            "SkateHarkinian/Props/QuarterMedium/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Quarter },
          { "Funbox", "Funbox", Collision::StaticSolid, Lifetime::Transient, 220, 60, 320, 1,
            "SkateHarkinian/Props/Funbox/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Funbox },
          { "QuarterTall", "Quarter Pipe - Tall", Collision::StaticSolid, Lifetime::Transient, 240, 180, 250, 1,
            "SkateHarkinian/Props/QuarterTall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Quarter },
          { "QuarterWide", "Quarter Pipe - Wide", Collision::StaticSolid, Lifetime::Transient, 360, 110, 180, 1,
            "SkateHarkinian/Props/QuarterWide/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Quarter },
          { "MiniLeft", "Mini Ramp - Left Half", Collision::StaticSolid, Lifetime::Transient, 180, 80, 200, 1,
            "SkateHarkinian/Props/MiniLeft/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Mini },
          { "MiniRight", "Mini Ramp - Right Half", Collision::StaticSolid, Lifetime::Transient, 180, 80, 200, 1,
            "SkateHarkinian/Props/MiniRight/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::MiniReverse },
          { "BankTall", "Bank - Tall", Collision::StaticSolid, Lifetime::Transient, 210, 110, 300, 1,
            "SkateHarkinian/Props/BankTall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Bank },
          { "BankWide", "Bank - Wide", Collision::StaticSolid, Lifetime::Transient, 360, 65, 220, 1,
            "SkateHarkinian/Props/BankWide/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Bank },
          { "BankLedge", "Bank-to-Ledge", Collision::StaticSolid, Lifetime::Transient, 200, 70, 280, 1,
            "SkateHarkinian/Props/BankLedge/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::BankLedge },
          { "Pyramid", "Pyramid", Collision::StaticSolid, Lifetime::Transient, 280, 65, 280, 1,
            "SkateHarkinian/Props/Pyramid/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Pyramid },
          { "HipLeft", "Hip - Left", Collision::StaticSolid, Lifetime::Transient, 200, 75, 220, 1,
            "SkateHarkinian/Props/HipLeft/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::HipLeft },
          { "HipRight", "Hip - Right", Collision::StaticSolid, Lifetime::Transient, 200, 75, 220, 1,
            "SkateHarkinian/Props/HipRight/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::HipRight },
          { "Spine", "Spine", Collision::StaticSolid, Lifetime::Transient, 180, 100, 240, 1,
            "SkateHarkinian/Props/Spine/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Spine },
          { "LaunchLong", "Launch Ramp - Long", Collision::StaticSolid, Lifetime::Transient, 120, 55, 260, 1,
            "SkateHarkinian/Props/LaunchLong/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Wood, Profile::Kicker },
          { "ManualLow", "Manual Pad - Low", Collision::StaticSolid, Lifetime::Transient, 180, 20, 260, 1,
            "SkateHarkinian/Props/ManualLow/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Pad },
          { "StairsSmall", "Stair Set - Small", Collision::StaticSolid, Lifetime::Transient, 180, 60, 200, 1,
            "SkateHarkinian/Props/StairsSmall/DisplayList", 0, 7, Category::Ramps, Behavior::SkateSurface,
            AudioSurface::Concrete, Profile::Stairs },
          { "FlatExtraLow", "Flat Rail - Extra Low", Collision::StaticSolid, Lifetime::Transient, 12, 12, 200, 1,
            "SkateHarkinian/Props/FlatExtraLow/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Flat },
          { "FlatExtraLong", "Flat Rail - Extra Long", Collision::StaticSolid, Lifetime::Transient, 12, 32, 520, 1,
            "SkateHarkinian/Props/FlatExtraLong/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Flat },
          { "RoundLong", "Round Rail - Long", Collision::StaticSolid, Lifetime::Transient, 10, 36, 420, 1,
            "SkateHarkinian/Props/RoundLong/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Round },
          { "RoundTall", "Round Rail - Tall", Collision::StaticSolid, Lifetime::Transient, 10, 60, 260, 1,
            "SkateHarkinian/Props/RoundTall/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Round },
          { "DownLong", "Down Rail - Long", Collision::StaticSolid, Lifetime::Transient, 12, 110, 480, 1,
            "SkateHarkinian/Props/DownLong/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Down },
          { "DownSteep", "Down Rail - Steep", Collision::StaticSolid, Lifetime::Transient, 12, 140, 260, 1,
            "SkateHarkinian/Props/DownSteep/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Down },
          { "UpRail", "Up Rail", Collision::StaticSolid, Lifetime::Transient, 12, 90, 300, 1,
            "SkateHarkinian/Props/UpRail/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::Up },
          { "Rainbow", "Rainbow Rail", Collision::StaticSolid, Lifetime::Transient, 10, 90, 360, 1,
            "SkateHarkinian/Props/Rainbow/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::Rainbow },
          { "DoubleKink", "Double Kink Rail", Collision::StaticSolid, Lifetime::Transient, 12, 110, 440, 1,
            "SkateHarkinian/Props/DoubleKink/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::DoubleKink },
          { "AFrame", "A-Frame Rail", Collision::StaticSolid, Lifetime::Transient, 12, 90, 320, 1,
            "SkateHarkinian/Props/AFrame/DisplayList", 0, 7, Category::Rails, Behavior::Grindable, AudioSurface::Metal,
            Profile::AFrame },
          { "FlatDown", "Flat-to-Down Rail", Collision::StaticSolid, Lifetime::Transient, 12, 90, 380, 1,
            "SkateHarkinian/Props/FlatDown/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::Kink },
          { "DownFlat", "Down-to-Flat Rail", Collision::StaticSolid, Lifetime::Transient, 12, 90, 380, 1,
            "SkateHarkinian/Props/DownFlat/DisplayList", 0, 7, Category::Rails, Behavior::Grindable,
            AudioSurface::Metal, Profile::DownFlat },
          { "PhysicsKickerSmall", "Kicker - Small (Physics)", Collision::StaticSolid, Lifetime::Transient, 90, 25, 100,
            1, "SkateHarkinian/Props/KickerSmall/DisplayList", 0, 7, Category::Props, Behavior::Movable,
            AudioSurface::Wood, Profile::Kicker },
          { "PhysicsKickerMedium", "Kicker - Medium (Physics)", Collision::StaticSolid, Lifetime::Transient, 120, 45,
            150, 1, "SkateHarkinian/Props/KickerMedium/DisplayList", 0, 7, Category::Props, Behavior::Movable,
            AudioSurface::Wood, Profile::Kicker },
          { "PhysicsBankSmall", "Bank - Small (Physics)", Collision::StaticSolid, Lifetime::Transient, 140, 40, 160, 1,
            "SkateHarkinian/Props/BankSmall/DisplayList", 0, 7, Category::Props, Behavior::Movable,
            AudioSurface::Concrete, Profile::Bank },
          { "PhysicsManualLow", "Manual Pad - Low (Physics)", Collision::StaticSolid, Lifetime::Transient, 140, 20, 180,
            1, "SkateHarkinian/Props/PhysicsManualLow/DisplayList", 0, 7, Category::Props, Behavior::Movable,
            AudioSurface::Concrete, Profile::Pad } }
    };
    return d;
}
struct Pose {
    float x = 0, y = 0, z = 0, yaw = 0, pitch = 0, roll = 0;
};
inline bool Valid(Pose p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::isfinite(p.yaw) &&
           std::isfinite(p.pitch) && std::isfinite(p.roll) && std::abs(p.x) < 28000 && std::abs(p.z) < 28000 &&
           std::abs(p.y) < 28000;
}
struct Instance {
    Id id;
    Type type;
    Pose pose;
    int collisionHandle = -1;
    bool attached = false;
    Pose markerPose{};
    float vx = 0, vy = 0, vz = 0;
    bool asleep = false;
};
struct Registry {
    std::array<std::optional<Instance>, kMaxObjects> slots{};
    std::vector<Id> undo;
    Id next = 1;
    size_t Count() const {
        size_t n = 0;
        for (auto& e : slots)
            n += e.has_value();
        return n;
    }
    int Find(Id id) const {
        for (int i = 0; i < kMaxObjects; ++i)
            if (slots[i] && slots[i]->id == id)
                return i;
        return -1;
    }
    int Add(Type type, Pose pose) {
        if ((int)type < 0 || (int)type >= kTypeCount || !Valid(pose) || next == UINT64_MAX)
            return -1;
        for (int i = 0; i < kMaxObjects; ++i)
            if (!slots[i]) {
                slots[i] = Instance{ next++, type, pose, -1 };
                undo.push_back(slots[i]->id);
                return i;
            }
        return -1;
    }
    void Remove(Id id) {
        int i = Find(id);
        if (i >= 0)
            slots[i].reset();
        for (auto it = undo.begin(); it != undo.end();)
            if (*it == id)
                it = undo.erase(it);
            else
                ++it;
    }
    Id Last() const {
        return undo.empty() ? 0 : undo.back();
    }
    void Clear() {
        for (auto& e : slots)
            e.reset();
        undo.clear();
    }
};
struct Point {
    float x, y, z;
};
struct Mesh {
    std::vector<Point> vertices;
    std::vector<std::array<uint16_t, 3>> triangles;
};

inline std::vector<Point> GrindPath(Type type) {
    const auto& d = Definitions()[(int)type];
    if (d.behavior != Behavior::Grindable)
        return {};
    float z = d.length / 2;
    if (d.profile == Profile::Up)
        return { { 0, 18, -z }, { 0, d.height, z } };
    if (d.profile == Profile::Rainbow) {
        std::vector<Point> out;
        for (int i = 0; i <= 12; ++i) {
            float f = i / 12.f;
            out.push_back({ 0, 18 + (d.height - 18) * std::sin(f * 3.14159265359f), -z + d.length * f });
        }
        return out;
    }
    if (d.profile == Profile::AFrame)
        return { { 0, 18, -z }, { 0, d.height, 0 }, { 0, 18, z } };
    if (d.profile == Profile::DoubleKink)
        return { { 0, d.height, -z },
                 { 0, d.height, -z * .6f },
                 { 0, d.height * .55f, 0 },
                 { 0, d.height * .55f, z * .4f },
                 { 0, 18, z } };
    if (d.profile == Profile::DownFlat)
        return { { 0, d.height, -z }, { 0, 18, 0 }, { 0, 18, z } };
    if (d.profile == Profile::Down)
        return { { 0, d.height, -z }, { 0, 18, z } };
    if (d.profile == Profile::Kink)
        return { { 0, d.height, -z }, { 0, d.height, 0 }, { 0, 18, z } };
    return { { 0, d.height, -z }, { 0, d.height, z } };
}
// Grind acquisition probes sit +/-0.09m from a path. A broad flat beam's
// centerline hits both sides and is correctly rejected as blocked. Expose its
// real top edges instead; mesh/collision dimensions and native tolerances stay unchanged.
inline std::vector<std::vector<Point>> GrindPaths(Type type) {
    const auto& d = Definitions()[(int)type];
    // Authored stationary prop edges share the existing multi-path provider.
    // Movable props and vert coping deliberately remain outside this provider.
    if (d.behavior == Behavior::SkateSurface) {
        float x = std::round(d.width / 2), z = std::round(d.length / 2), h = std::round(d.height);
        std::vector<std::vector<Point>> paths;
        auto sides = [&](float from, float to, float height) {
            for (float side : { -1.f, 1.f })
                paths.push_back({ { side * x, height, from }, { side * x, height, to } });
        };
        if (d.profile == Profile::Pad)
            sides(-z, z, h);
        else if (d.profile == Profile::Funbox)
            sides(std::round(-d.length / 2 + d.length / 3), std::round(d.length / 2 - d.length / 3), h);
        else if (d.profile == Profile::BankLedge)
            sides(0, z, h);
        else if (d.profile == Profile::Stairs) {
            // Each tread is a real horizontal edge. Do not create an invisible
            // diagonal above the stepped collision or a grind down a vertical riser.
            for (int i = 0; i < 4; ++i)
                sides(std::round(-d.length / 2 + d.length * i / 4) + 2,
                      std::round(-d.length / 2 + d.length * (i + 1) / 4) - 2, std::round(d.height * (i + 1) / 4));
        }
        return paths;
    }
    auto center = GrindPath(type);
    if (center.empty())
        return {};
    if (d.profile == Profile::Round)
        return { center };
    if (d.profile == Profile::Rainbow) {
        std::vector<std::vector<Point>> out;
        for (float side : { -1.f, 1.f }) {
            auto edge = center;
            for (auto& p : edge) {
                p.x = side * d.width * .35355339f;
                p.y -= d.width * .14644661f;
            }
            out.push_back(std::move(edge));
        }
        return out;
    }
    std::vector<std::vector<Point>> paths;
    for (float side : { -1.f, 1.f }) {
        auto edge = center;
        for (auto& p : edge)
            p.x = side * d.width / 2;
        paths.push_back(std::move(edge));
    }
    return paths;
}
inline Mesh Geometry(Type type) {
    const auto& d = Definitions()[(int)type];
    float x = d.width / 2, z = d.length / 2, h = d.height;
    Mesh m;
    auto quad = [&](int ai, int bi, int ci, int ei) {
        const auto a = static_cast<uint16_t>(ai), b = static_cast<uint16_t>(bi), c = static_cast<uint16_t>(ci),
                   e = static_cast<uint16_t>(ei);
        m.triangles.push_back({ a, b, c });
        m.triangles.push_back({ a, c, e });
    };
    auto box = [&](float loX, float hiX, float loY, float hiY, float loZ, float hiZ) {
        uint16_t start = (uint16_t)m.vertices.size();
        for (int i = 0; i < 8; ++i)
            m.vertices.push_back({ i & 1 ? hiX : loX, i & 2 ? hiY : loY, i & 4 ? hiZ : loZ });
        const int f[6][4] = { { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 2, 3, 1 },
                              { 4, 5, 7, 6 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
        for (auto& a : f)
            quad(start + a[0], start + a[1], start + a[2], start + a[3]);
    };
    if (d.behavior == Behavior::Grindable) {
        auto path = GrindPath(type);
        int sides = (d.profile == Profile::Round || d.profile == Profile::Rainbow) ? 8 : 4;
        // Octagonal/rectangular beam cross sections with exact top centerline.
        for (auto p : path) {
            if (sides == 4) {
                m.vertices.insert(m.vertices.end(),
                                  { { -x, p.y - 6, p.z }, { x, p.y - 6, p.z }, { x, p.y, p.z }, { -x, p.y, p.z } });
            } else
                for (int i = 0; i < 8; ++i) {
                    float a = i * 6.283185307f / 8;
                    m.vertices.push_back({ -std::sin(a) * x, p.y - x + std::cos(a) * x, p.z });
                }
        }
        for (int j = 0; j < (int)path.size() - 1; ++j)
            for (int i = 0; i < sides; ++i) {
                int k = (i + 1) % sides;
                quad(j * sides + i, j * sides + k, (j + 1) * sides + k, (j + 1) * sides + i);
            }
        for (int i = 1; i < sides - 1; ++i) {
            m.triangles.push_back({ 0, (uint16_t)(i + 1), (uint16_t)i });
            uint16_t off = (uint16_t)((path.size() - 1) * sides);
            m.triangles.push_back({ off, (uint16_t)(off + i), (uint16_t)(off + i + 1) });
        }
        for (float f : { .15f, .85f }) {
            float pz = -z + d.length * f, py = h;
            for (size_t j = 1; j < path.size(); ++j)
                if (pz >= path[j - 1].z && pz <= path[j].z) {
                    float t = (pz - path[j - 1].z) / (path[j].z - path[j - 1].z);
                    py = path[j - 1].y + (path[j].y - path[j - 1].y) * t;
                    break;
                }
            box(-2, 2, 0, py - 6, pz - 2, pz + 2);
            box(-12, 12, 0, 2, pz - 10, pz + 10);
        }
    } else if (d.profile == Profile::Pad) {
        box(-x, x, 0, h, -z, z);
    } else if (d.profile == Profile::Pyramid || d.profile == Profile::HipLeft || d.profile == Profile::HipRight) {
        // A bounded four-panel pyramid / three-panel hip. Shared vertices, upward winding.
        m.vertices = { { -x, 0, -z }, { x, 0, -z }, { x, 0, z }, { -x, 0, z }, { 0, h, 0 } };
        if (d.profile == Profile::HipLeft)
            m.vertices[4].x = -x;
        if (d.profile == Profile::HipRight)
            m.vertices[4].x = x;
        m.triangles = { { 0, 4, 1 }, { 1, 4, 2 }, { 2, 4, 3 }, { 3, 4, 0 }, { 0, 1, 2 }, { 0, 2, 3 } };
    } else if (d.behavior == Behavior::SkateSurface || d.behavior == Behavior::Movable && d.profile != Profile::Box) {
        std::vector<Point> profile;
        int steps = d.profile == Profile::Quarter                                       ? ((int)type >= 23 ? 12 : 8)
                    : (d.profile == Profile::Mini || d.profile == Profile::MiniReverse) ? 12
                    : d.profile == Profile::Kicker                                      ? 6
                    : d.profile == Profile::Funbox                                      ? 3
                                                                                        : 1;
        if (d.profile == Profile::BankLedge) {
            profile = { { 0, 0, -z }, { 0, h, 0 }, { 0, h, z } };
            steps = 2;
        } else if (d.profile == Profile::Spine) {
            profile = { { 0, 0, -z }, { 0, h, -8 }, { 0, h, 8 }, { 0, 0, z } };
            steps = 3;
        } else if (d.profile == Profile::Stairs) {
            for (int i = 0; i < 4; ++i) {
                float zz = -z + d.length * i / 4;
                profile.push_back({ 0, h * i / 4, zz });
                profile.push_back({ 0, h * (i + 1) / 4, zz });
            }
            profile.push_back({ 0, h, z });
            steps = (int)profile.size() - 1;
        } else
            for (int i = 0; i <= steps; ++i) {
                float f = (float)i / steps, y = h * f;
                if (d.profile == Profile::Quarter || d.profile == Profile::Mini || d.profile == Profile::MiniReverse)
                    y = h * (1 - std::sqrt(std::max(0.f, 1 - f * f)));
                else if (d.profile == Profile::Kicker)
                    y = h * f * f;
                else if (d.profile == Profile::Funbox)
                    y = i == 0 || i == steps ? 0 : h;
                if (d.profile == Profile::Mini || d.profile == Profile::MiniReverse) {
                    float t = d.profile == Profile::Mini ? 1 - f : f;
                    y = h * (1 - std::sqrt(std::max(0.f, 1 - t * t)));
                }
                profile.push_back({ 0, y, -z + d.length * f });
            }
        // Closed prism, +Z forward; strip normals face upward.
        for (auto p : profile)
            m.vertices.insert(m.vertices.end(), { { -x, p.y, p.z }, { x, p.y, p.z } });
        uint16_t b = (uint16_t)m.vertices.size();
        m.vertices.insert(m.vertices.end(), { { -x, 0, -z }, { x, 0, -z }, { -x, 0, z }, { x, 0, z } });
        for (int i = 0; i < steps; ++i)
            quad(i * 2, (i + 1) * 2, (i + 1) * 2 + 1, i * 2 + 1);
        for (int i = 0; i < steps; ++i) {
            uint16_t q = (uint16_t)m.vertices.size();
            m.vertices.insert(m.vertices.end(), { { -x, 0, profile[i].z },
                                                  { x, 0, profile[i].z },
                                                  { -x, 0, profile[i + 1].z },
                                                  { x, 0, profile[i + 1].z } });
            quad(q, q + 1, q + 3, q + 2);
        }
        if (profile.front().y > 0)
            quad(b, 0, 1, b + 1);
        if (profile.back().y > 0)
            quad(b + 2, b + 3, steps * 2 + 1, steps * 2);
        // Side strips follow the curved profile rather than spanning it with a fan.
        for (int i = 0; i < steps; ++i)
            for (int side = 0; side < 2; ++side) {
                uint16_t aa = (uint16_t)(i * 2 + side), bb = (uint16_t)((i + 1) * 2 + side),
                         ba = (uint16_t)m.vertices.size();
                m.vertices.push_back({ side ? x : -x, 0, profile[i].z });
                m.vertices.push_back({ side ? x : -x, 0, profile[i + 1].z });
                if (!side) {
                    if (profile[i + 1].y > 0)
                        m.triangles.push_back({ ba, (uint16_t)(ba + 1), bb });
                    if (profile[i].y > 0)
                        m.triangles.push_back({ ba, bb, aa });
                } else {
                    if (profile[i].y > 0)
                        m.triangles.push_back({ ba, aa, bb });
                    if (profile[i + 1].y > 0)
                        m.triangles.push_back({ ba, bb, (uint16_t)(ba + 1) });
                }
            }
    } else
        box(-x, x, 0, h, -z, z);
    for (auto& v : m.vertices) {
        v.x = std::round(v.x);
        v.y = std::round(v.y);
        v.z = std::round(v.z);
    }
    for (auto it = m.triangles.begin(); it != m.triangles.end();) {
        auto a = m.vertices[(*it)[0]], b = m.vertices[(*it)[1]], c = m.vertices[(*it)[2]];
        float nx = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
              ny = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
              nz = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (nx * nx + ny * ny + nz * nz < .001f)
            it = m.triangles.erase(it);
        else
            ++it;
    }
    return m;
}
} // namespace NativeSkateObjects

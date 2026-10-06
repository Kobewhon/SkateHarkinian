#pragma once
#include "NativeSkateObjectRegistry.h"
#include <algorithm>
namespace NativeSkateObjectEditor {
using namespace NativeSkateObjects;
enum class Stage { Root, Browser, Manipulating };
// Existing native-pad semantic face bits, not device IDs.
constexpr uint16_t kSnapAction = 0x4000, kRecallAction = 0x8000;
constexpr uint16_t kDeleteAction = kSnapAction; // X/Square; browser only, snap remains placement-only.
constexpr float kMoveSpeed = 100.f, kTranslationMultiplier = 2.f, kRotationSpeed = 1.25f, kCameraSpeed = 1.3f,
                kZoomSpeed = 140.f;
inline float Axis(float x) {
    x = std::clamp(x, -1.f, 1.f);
    return std::abs(x) < .15f ? 0.f : x * x * x;
}
inline Point Rotate(Point p, Pose pose) { // Same Y-X-Z basis as OoT DynaPoly.
    float c = std::cos(pose.roll), s = std::sin(pose.roll);
    Point a{ c * p.x - s * p.y, s * p.x + c * p.y, p.z };
    c = std::cos(pose.pitch);
    s = std::sin(pose.pitch);
    Point b{ a.x, c * a.y - s * a.z, s * a.y + c * a.z };
    c = std::cos(pose.yaw);
    s = std::sin(pose.yaw);
    return { c * b.x + s * b.z, b.y, -s * b.x + c * b.z };
}
struct Camera {
    float yaw = 0, pitch = .45f, distance = 240.f;
    void Read(float x, float y, int zoom, float dt) {
        yaw = std::remainder(yaw - Axis(x) * kCameraSpeed * dt, 6.2831853f);
        pitch = std::clamp(pitch + Axis(y) * kCameraSpeed * dt, .08f, 1.35f);
        distance = std::clamp(distance - zoom * kZoomSpeed * dt, 90.f, 700.f);
    }
    Point Eye(Point at) const {
        float h = std::cos(pitch) * distance;
        return { at.x - std::sin(yaw) * h, at.y + std::sin(pitch) * distance, at.z - std::cos(yaw) * h };
    }
};
// OoT camera looks along at-eye. Camera-right is forward cross world-up.
struct MovementBasis {
    Point forward{ 0, 0, 1 };
    void Read(Point eye, Point at) {
        float x = at.x - eye.x, z = at.z - eye.z, length = std::hypot(x, z);
        if (std::isfinite(length) && length > 1e-4f)
            forward = { x / length, 0, z / length };
    }
    void Move(Pose& p, float x, float y, float speed, float dt) const {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dt))
            return;
        float length = std::hypot(x, y);
        if (length <= .15f)
            return;
        // Radial cubic response preserves fine control and caps diagonal speed.
        float magnitude = std::pow(std::min(length, 1.f), 3.f);
        x = x / length * magnitude;
        y = y / length * magnitude;
        p.x = std::clamp(p.x + (-forward.z * x + forward.x * y) * speed * dt, -27000.f, 27000.f);
        p.z = std::clamp(p.z + (forward.x * x + forward.z * y) * speed * dt, -27000.f, 27000.f);
    }
};
struct StickOwnership {
    bool rotation = false;
    void Read(bool modifier, float, float) {
        rotation = modifier;
    }
    bool BlocksCamera() const {
        return rotation;
    }
};
inline void Turn(Pose& p, float x, int axis, float dt) {
    float* v = axis == 0 ? &p.yaw : axis == 1 ? &p.pitch : &p.roll;
    *v = std::remainder(*v + Axis(x) * kRotationSpeed * dt, 6.2831853f);
}
inline const char* Name(Category c) {
    constexpr const char* names[] = { "Ramps", "Rails", "Ledges", "Props", "Special", "Debug", "Placed Objects" };
    return names[(int)c];
}
} // namespace NativeSkateObjectEditor

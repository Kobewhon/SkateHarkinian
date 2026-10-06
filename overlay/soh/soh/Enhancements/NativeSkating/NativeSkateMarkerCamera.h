#pragma once
#include <cmath>
namespace NativeSkateSessionMarker {
struct CameraPose {
    bool valid = false, manual = false;
    float yaw = 0, pitch = 0, distance = 0, fov = 60;
    int zoomSetting = 0;
    short roll = 0;
    float targetOffset[3]{};
    bool Capture(const float* player, const float* at, const float* eye) {
        valid = false;
        float dx = eye[0] - at[0], dy = eye[1] - at[1], dz = eye[2] - at[2];
        float horizontal = std::hypot(dx, dz);
        distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (!std::isfinite(distance) || distance < 1 || distance > 10000)
            return false;
        yaw = std::atan2(dx, dz);
        pitch = std::atan2(dy, horizontal);
        for (int i = 0; i < 3; ++i) {
            targetOffset[i] = at[i] - player[i];
            if (!std::isfinite(targetOffset[i]))
                return false;
        }
        valid = true;
        return true;
    }
    void Restore(const float* player, float* at, float* eye) const {
        for (int i = 0; i < 3; ++i)
            at[i] = player[i] + targetOffset[i];
        float horizontal = std::cos(pitch) * distance;
        eye[0] = at[0] + std::sin(yaw) * horizontal;
        eye[1] = at[1] + std::sin(pitch) * distance;
        eye[2] = at[2] + std::cos(yaw) * horizontal;
    }
};
} // namespace NativeSkateSessionMarker

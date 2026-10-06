#pragma once
#include <cstdint>
#include "NativeSkateProxyGeometry.h"
// Presentation only. Resources are the user's normal OoT standalone shield models.
namespace NativeSkateBoardAppearance {
enum class Style : int { Default = 0, DekuShield = 1, HylianShield = 2, MirrorShield = 3 };
inline Style Resolve(int value) {
    return value >= 0 && value <= 3 ? static_cast<Style>(value) : Style::Default;
}
struct Vec3 {
    float x, y, z;
};
struct BoardVisual {
    Style style;
    const char* name;
    const char* model;
    const char* secondary;
    float scale;
    Vec3 offset, rotation;
    float frontZ, backZ, hardwareY;
};
inline BoardVisual DefaultVisual() {
    return { Style::Default,
             "Default Skateboard",
             "SkateHarkinian/Board/Default/BuiltinMesh",
             nullptr,
             1,
             { 0, 0, 0 },
             { 0, 0, 0 },
             .242f,
             -.242f,
             0 };
}
inline BoardVisual ResolveBoardVisual(int requested) {
    switch (Resolve(requested)) {
        case Style::DekuShield:
            return { Style::DekuShield,
                     "Deku Shield",
                     "objects/object_gi_shield_1/gGiDekuShieldDL",
                     nullptr,
                     .012295082f,
                     { 0, -.049180328f, .006147541f },
                     { -1.570796327f, 0, 0 },
                     .210f,
                     -.225f,
                     -.05147541f };
        case Style::HylianShield:
            return { Style::HylianShield,
                     "Hylian Shield",
                     "objects/object_gi_shield_2/gGiHylianShieldDL",
                     nullptr,
                     .011875f,
                     { 0, -.07125f, 0 },
                     { -1.570796327f, 0, 0 },
                     .235f,
                     -.235f,
                     -.06125f };
        case Style::MirrorShield:
            return { Style::MirrorShield,
                     "Mirror Shield",
                     "objects/object_gi_shield_3/gGiMirrorShieldDL",
                     "objects/object_gi_shield_3/gGiMirrorShieldSymbolDL",
                     .0121875f,
                     { 0, -.09140625f, 0 },
                     { -1.570796327f, 0, 0 },
                     .240f,
                     -.240f,
                     -.08140625f };
        default:
            return DefaultVisual();
    }
}
inline BoardVisual WithResourceStatus(BoardVisual v, bool resolved) {
    return v.style == Style::Default || resolved ? v : DefaultVisual();
}
inline NativeSkateProxyGeometry::Shape HardwareShape(BoardVisual v, NativeSkateProxyGeometry::Shape original) {
    if (v.style != Style::Default) {
        original.frontZ = v.frontZ;
        original.backZ = v.backZ;
    }
    return original;
}
} // namespace NativeSkateBoardAppearance

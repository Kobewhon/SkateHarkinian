#include "NativeSkateBoardAppearance.h"
#include "NativeSkateBoardModel.h"
#include <cassert>
#include <set>
#include <string>
#include <cstdio>

#include <algorithm>
#include <cmath>
#include <vector>
#include "NativeSkateProxyGeometry.h"
// Original built-in presentation mesh. Meter-authored, deck center origin;
// +X axle/right, +Y up, +Z nose/forward. No physics or actor transforms here.
namespace LegacyBoardModel {
struct Vertex {
    float x, y, z;
};
struct Color {
    unsigned char r, g, b;
};
struct Triangle {
    Vertex a, b, c;
    Color color;
};
inline float Bound(float v, float fallback, float lo, float hi) {
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}
struct Scale {
    float global, age, final;
};
inline Scale VisualScale(bool adult, float global, float adultScale, float childScale) {
    Scale s{ Bound(global, 1, .5f, 1.25f), Bound(adult ? adultScale : childScale, adult ? 1.f : .8f, .5f, 1.25f), 0 };
    s.final = s.global * s.age;
    return s;
}
inline std::vector<Triangle> Build(const NativeSkateProxyGeometry::Shape& shape) {
    std::vector<Triangle> m;
    m.reserve(500);
    auto tri = [&](Vertex a, Vertex b, Vertex c, Color color) { m.push_back({ a, b, c, color }); };
    auto quad = [&](Vertex a, Vertex b, Vertex c, Vertex d, Color color) {
        tri(a, b, c, color);
        tri(a, c, d, color);
    };
    const Color grip{ 29, 32, 34 }, wood{ 173, 126, 72 }, edge{ 212, 164, 99 }, metal{ 161, 169, 179 };
    const float z[] = { -.4f, -.385f, -.35f, -.30f, -.24f, 0, .24f, .30f, .35f, .385f, .4f };
    const float w[] = { .008f, .045f, .08f, .095f, .10f, .10f, .10f, .095f, .08f, .045f, .008f };
    auto rise = [](float t) {
        float k = std::max(0.f, (std::abs(t) - .27f) / .13f);
        return .027f * k * k;
    };
    for (int i = 0; i < 10; ++i) {
        float a = rise(z[i]), b = rise(z[i + 1]);
        quad({ -w[i], a + .01f, z[i] }, { w[i], a + .01f, z[i] }, { w[i + 1], b + .01f, z[i + 1] },
             { -w[i + 1], b + .01f, z[i + 1] }, grip);
        quad({ w[i], a - .01f, z[i] }, { -w[i], a - .01f, z[i] }, { -w[i + 1], b - .01f, z[i + 1] },
             { w[i + 1], b - .01f, z[i + 1] }, wood);
        for (int side : { -1, 1 })
            quad({ side * w[i], a - .01f, z[i] }, { side * w[i], a + .01f, z[i] },
                 { side * w[i + 1], b + .01f, z[i + 1] }, { side * w[i + 1], b - .01f, z[i + 1] }, edge);
    }
    for (int i : { 0, 10 }) {
        float y = rise(z[i]);
        quad({ -w[i], y - .01f, z[i] }, { w[i], y - .01f, z[i] }, { w[i], y + .01f, z[i] }, { -w[i], y + .01f, z[i] },
             edge);
    }
    auto box = [&](float x, float y, float z, float dx, float dy, float dz) {
        Vertex v[8];
        for (int i = 0; i < 8; ++i)
            v[i] = { x + (i & 1 ? dx : -dx) / 2, y + (i & 2 ? dy : -dy) / 2, z + (i & 4 ? dz : -dz) / 2 };
        const int f[6][4] = { { 0, 1, 3, 2 }, { 4, 6, 7, 5 }, { 0, 4, 5, 1 },
                              { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 5, 7, 3 } };
        for (auto& a : f)
            quad(v[a[0]], v[a[1]], v[a[2]], v[a[3]], metal);
    };
    for (float t : { shape.frontZ, shape.backZ }) {
        box(0, -.021f, t, .07f, .015f, .06f);                                                // baseplate
        box(0, (shape.wheelY - .021f) / 2, t, .025f, std::abs(shape.wheelY + .021f), .025f); // hanger
        box(0, shape.wheelY, t, shape.wheelX * 2, .013f, .013f);                             // axle
        for (int side : { -1, 1 }) {
            const float x = side * shape.wheelX, half = .0125f;
            for (int i = 0; i < 12; ++i) {
                const float a = i * 6.28318530718f / 12, b = (i + 1) * 6.28318530718f / 12;
                auto ring = [&](float xx, float angle) {
                    return Vertex{ xx, shape.wheelY + shape.radius * std::cos(angle),
                                   t + shape.radius * std::sin(angle) };
                };
                const unsigned char shade = static_cast<unsigned char>(180 + 25 * std::cos((a + b) / 2));
                quad(ring(x - half, a), ring(x + half, a), ring(x + half, b), ring(x - half, b),
                     { shade, shade, static_cast<unsigned char>(shade - 12) });
                for (int end : { -1, 1 })
                    tri({ x + end * half, shape.wheelY, t }, ring(x + end * half, a), ring(x + end * half, b),
                        { 219, 215, 198 });
            }
        }
    }
    return m;
}
} // namespace LegacyBoardModel

int main() {
    using namespace NativeSkateBoardAppearance;
    NativeSkateProxyGeometry::Shape shape;
    auto old = LegacyBoardModel::Build(shape);
    auto current = NativeSkateBoardModel::Build(shape);
    assert(old.size() == current.size());
    for (size_t i = 0; i < old.size(); ++i) {
        auto a = old[i];
        auto b = current[i];
        assert(a.a.x == b.a.x && a.a.y == b.a.y && a.a.z == b.a.z && a.b.x == b.b.x && a.b.y == b.b.y &&
               a.b.z == b.b.z && a.c.x == b.c.x && a.c.y == b.c.y && a.c.z == b.c.z && a.color.r == b.color.r &&
               a.color.g == b.color.g && a.color.b == b.color.b);
    }
    std::set<std::string> unique;
    for (int i = 0; i < 4; ++i) {
        auto v = ResolveBoardVisual(i);
        assert((int)v.style == i);
        unique.insert(v.model);
        assert(WithResourceStatus(v, false).style == Style::Default);
        if (!i)
            continue;
        auto h = HardwareShape(v, shape);
        auto mesh = NativeSkateBoardModel::Build(h, false, v.hardwareY);
        assert(mesh.size() == 264);
        assert(v.scale > 0 && v.scale < .02 && v.rotation.x < 0);
        assert(h.radius == shape.radius && h.wheelX == shape.wheelX && h.wheelY == shape.wheelY);
        assert(WithResourceStatus(v, true).style == v.style);
        // Each hardware assembly retains two trucks and four twelve-segment wheels.
        assert(mesh.size() + 84 == current.size());
        assert(mesh[0].color.r == 161);
    }
    assert(unique.size() == 4);
    assert(Resolve(-1) == Style::Default && Resolve(99) == Style::Default);
    puts(
        "PASS four real-model definitions, invalid/missing fallback, unchanged default geometry/colors, two trucks/four wheels, cosmetic-only anchors");
}

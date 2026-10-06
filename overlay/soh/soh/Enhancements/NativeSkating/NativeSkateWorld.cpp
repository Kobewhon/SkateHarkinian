#include "NativeSkateWorld.h"
#include "NativeSkateUnits.h"
#include "NativeSkateCache.h"
#include "NativeSkatePerf.h"
#include <tuple>
#include <algorithm>
#include <array>
#include <cmath>
extern "C" {
#include "global.h"
}

namespace NativeSkateWorld {
static NativeSkateCache::Bounded<std::tuple<int, const void*, uint64_t>, Snapshot> cache;
void ClearCache() {
    cache.Clear();
}
size_t CacheEntries() {
    return cache.Count();
}
size_t CacheBytes() {
    return cache.Bytes();
}
// Exact Kokiri plank slab vertices; pillars, stream banks and other bridges
// cannot match. Keep the deck solid above; native off-board ledge/contact
// queries must not treat this overhead slab as a reachable ground obstacle.
static bool KokiriPlankVertex(const Vec3s& p) {
    static constexpr int points[][3] = { { 201, 130, 59 },  { 432, 120, 121 }, { 211, 130, 20 },  { 443, 120, 82 },
                                         { 56, 180, -21 },  { 46, 180, 18 },   { 302, 100, 687 }, { 374, 85, 571 },
                                         { 335, 85, 561 },  { 340, 100, 697 }, { 485, 120, 156 }, { 447, 120, 146 },
                                         { 447, 100, 146 }, { 335, 65, 561 },  { 302, 80, 687 },  { 340, 80, 697 },
                                         { 374, 65, 571 },  { 485, 100, 156 }, { 201, 110, 59 },  { 432, 100, 121 },
                                         { 46, 160, 18 },   { 211, 110, 20 },  { 56, 160, -21 },  { 443, 100, 82 } };
    for (const auto& v : points)
        if (p.x == v[0] && p.y == v[1] && p.z == v[2])
            return true;
    return false;
}
bool Capture(const PlayState* play, Snapshot& out) {

    out = {};
    if (!play || !play->colCtx.colHeader) {
        out.error = "WAITING_FOR_WORLD";
        return false;
    }
    const CollisionHeader* header = play->colCtx.colHeader;
    if (header->numPolygons && (!header->polyList || !header->vtxList)) {
        out.error = "Invalid OoT static collision header";
        return false;
    }
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](uint64_t v) {
        for (int b = 0; b < 8; ++b) {
            hash ^= (v >> (8 * b)) & 255;
            hash *= 1099511628211ull;
        }
    };
    mix(1);
    mix(header->numVertices);
    mix(header->numPolygons);
    mix(header->minBounds.y);
    if (header->numVertices && !header->vtxList) {
        out.error = "Invalid static vertices";
        return false;
    }
    for (unsigned i = 0; i < header->numVertices; ++i) {
        auto v = header->vtxList[i];
        mix(v.x);
        mix(v.y);
        mix(v.z);
    }
    for (unsigned i = 0; i < header->numPolygons; ++i) {
        const auto& p = header->polyList[i];
        mix(p.type);
        for (auto index : p.vtxData)
            mix(index);
        mix(p.normal.x);
        mix(p.normal.y);
        mix(p.normal.z);
        mix(p.dist);
    }
    auto key = std::make_tuple((int)play->sceneNum, (const void*)header, hash);
    if (auto hit = cache.Find(key)) {
        out = *hit;
        return true;
    }
    out.identity = hash;
    out.floorY = play->colCtx.colHeader->minBounds.y;
    for (size_t i = 0; i < header->numPolygons; ++i) {
        const CollisionPoly& poly = header->polyList[i];
        if (poly.flags_vIA & 0x4000)
            continue;
        ++out.candidate;
        std::array<std::array<float, 3>, 3> v{};
        bool valid = true;
        for (int j = 0; j < 3; ++j) {
            size_t index = COLPOLY_VTX_INDEX(poly.vtxData[j]);
            if (index >= header->numVertices) {
                valid = false;
                break;
            }
            const auto& p = header->vtxList[index];
            const float host[3] = { (float)p.x, (float)p.y, (float)p.z };
            for (float f : host)
                if (!std::isfinite(f) || std::abs(f) > 1000000.0f)
                    valid = false;
            if (!valid || !NativeSkateUnits::Anchor::ToRust(host, v[j].data())) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            ++out.rejected;
            continue;
        }
        const float ax = v[1][0] - v[0][0], ay = v[1][1] - v[0][1], az = v[1][2] - v[0][2];
        const float bx = v[2][0] - v[0][0], by = v[2][1] - v[0][1], bz = v[2][2] - v[0][2];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        if (nx * nx + ny * ny + nz * nz < 1e-12f) {
            ++out.degenerate;
            ++out.rejected;
            continue;
        }
        const float sx = COLPOLY_GET_NORMAL(poly.normal.x), sy = COLPOLY_GET_NORMAL(poly.normal.y),
                    sz = COLPOLY_GET_NORMAL(poly.normal.z);
        if (nx * sx + ny * sy + nz * sz < 0.0f)
            std::swap(v[1], v[2]);
        bool plank = play->sceneNum == SCENE_KOKIRI_FOREST;
        for (int j = 0; j < 3 && plank; ++j)
            plank = KokiriPlankVertex(header->vtxList[COLPOLY_VTX_INDEX(poly.vtxData[j])]);
        if (plank) {
            for (const auto& p : v)
                for (float f : p)
                    out.kokiriWalkway.push_back(f);
            continue;
        }
        for (const auto& p : v)
            for (int axis = 0; axis < 3; ++axis) {
                if (out.accepted == 0 && out.triangles.empty())
                    out.min[axis] = out.max[axis] = p[axis];
                else {
                    out.min[axis] = std::min(out.min[axis], p[axis]);
                    out.max[axis] = std::max(out.max[axis], p[axis]);
                }
                out.triangles.push_back(p[axis]);
            }
        ++out.accepted;
    }
    if (!out.accepted) {
        out.error = "NO_VALID_COLLISION_GEOMETRY";
        return false;
    }
    cache.Put(key, out, sizeof(out) + (out.triangles.capacity() + out.kokiriWalkway.capacity()) * sizeof(float));
    return true;
}
} // namespace NativeSkateWorld

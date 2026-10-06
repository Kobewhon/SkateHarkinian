#include "NativeSkateObjectRegistry.h"
#include "NativeSkateObjectEditor.h"
#include <cassert>
#include <set>
#include <string>
#include <cstdio>
int main() {
    using namespace NativeSkateObjects;
    std::set<std::string> ids;
    int rails = 0, ramps = 0, movable = 0, edges = 0;
    for (int i = 0; i < kTypeCount; ++i) {
        auto& d = Definitions()[i];
        assert(ids.insert(d.id).second);
        auto m = Geometry((Type)i);
        assert(!m.triangles.empty());
        for (auto v : m.vertices)
            assert(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
                   std::abs(v.x) <= d.width / 2 + 12 && v.y >= 0 && v.y <= d.height &&
                   std::abs(v.z) <= d.length / 2 + 10);
        for (auto t : m.triangles)
            for (auto index : t)
                assert(index < m.vertices.size());
        auto paths = GrindPaths((Type)i);
        if (d.behavior == Behavior::Grindable) {
            ++rails;
            assert(!paths.empty());
        } else if (!paths.empty()) {
            ++edges;
        }
        for (auto path : paths) {
            assert(path.size() > 1);
            for (auto p : path)
                assert(std::isfinite(p.y));
            for (float yaw : { 0.f, .785398f, 1.570796f, 3.14159f, 4.71239f })
                for (float pitch : { -.25f, 0.f, .25f })
                    for (auto p : path) {
                        auto v = NativeSkateObjectEditor::Rotate(p, { 0, 100, 200, yaw, pitch, 0 });
                        assert(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z));
                    }
        }
        if (d.category == Category::Ramps)
            ++ramps;
        if (d.behavior == Behavior::Movable && d.category != Category::Debug)
            ++movable;
    }
    assert(kTypeCount == 53 && rails == 22 && ramps == 24 && movable == 4 && edges == 4);
    printf(
        "PASS53 unique definitions,22 rails,4 authored edge props,24 ramps,4 optional physics props; bounded meshes,valid indices,local paths and15 orientations\n");
}

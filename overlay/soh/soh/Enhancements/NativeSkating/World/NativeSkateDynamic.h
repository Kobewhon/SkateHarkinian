#pragma once
#include "../NativeSkateRuntime.h"
#include "NativeSkateGrindCompiler.h"
#include "../NativeSkateCache.h"
#include <array>
#include <map>
struct PlayState;
struct Actor;

namespace NativeSkateDynamic {
struct Mesh {
    const Actor* actor = nullptr;
    const void* header = nullptr;
    std::vector<float> local;
    std::array<float, 16> frame{};
    NativeSkateGrindCompiler::Result grinds;
    bool grindsEnabled = false;
    uint64_t objectId = 0;
};
class World {
    std::map<uint32_t, Mesh> meshes;
    NativeSkateCache::Bounded<std::pair<const void*, uint64_t>, Mesh> resources{ 32, 8 * 1024 * 1024 };

  public:
    void ActorInit(Actor* actor) {
        for (auto& entry : meshes)
            if (entry.second.actor == actor)
                entry.second.actor = nullptr;
    }
    size_t MeshCount() const {
        return meshes.size();
    }
    size_t ResourceCount() const {
        return resources.Count();
    }
    size_t ResourceBytes() const {
        return resources.Bytes();
    }
    size_t GrindCount() const {
        size_t n = 0;
        for (const auto& m : meshes)
            n += m.second.grinds.splines.size();
        return n;
    }
    unsigned updates = 0, triangles = 0;
    double updateMs = 0;
    void Clear() {
        meshes.clear();
        updates = triangles = 0;
    }
    void ClearResources() {
        Clear();
        resources.Clear();
    }
    bool Update(const PlayState*, NativeSkateRuntime::Session&);
    void Draw(PlayState*, NativeSkateRuntime::Session&) const;
};
} // namespace NativeSkateDynamic

#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct PlayState;
namespace NativeSkateWorld {
struct Snapshot {
    std::vector<float> triangles;
    std::vector<float> kokiriWalkway; // native contact ownership only; stock OoT geometry is untouched
    uint32_t candidate = 0, accepted = 0, rejected = 0, degenerate = 0;
    float min[3] = {}, max[3] = {};
    float floorY = 0.0f;
    std::string error;
    uint64_t identity = 0;
};
void ClearCache();
size_t CacheEntries();
size_t CacheBytes();
bool Capture(const PlayState* play, Snapshot& result);
} // namespace NativeSkateWorld

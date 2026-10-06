#pragma once
#include "NativeSkateRuntime.h"
#include <map>
#include <array>
#include <set>
struct PlayState;
struct Actor;
namespace NativeSkateActors {
inline bool Onboard(uint32_t state) {
    return state == 1000 || state == 1001 || (state >= 100 && state <= 105) || (state >= 200 && state <= 202) ||
           (state >= 400 && state <= 405) || (state >= 600 && state <= 602);
}
struct Proxy {
    const void* actor = nullptr;
    const void* collider = nullptr;
    int element = 0, category = 0, actorId = 0, shape = 0, at = 0, ac = 0, oc = 0;
    std::vector<float> local;
    std::array<float, 16> frame{};
    bool seen = false;
    uint64_t tick = 0;
    float velocity[3] = {};
};
class World {
    std::map<unsigned, Proxy> objects;
    std::set<unsigned> previousContacts;
    uint64_t registryTick = ~uint64_t(0);

  public:
    double ms = 0, maximum = 0, total = 0;
    uint64_t samples = 0;
    unsigned updates = 0;
    bool Update(PlayState*, NativeSkateRuntime::Session&, uint64_t tick);
    void Clear(NativeSkateRuntime::Session&);
    void ActorInit(const Actor*, NativeSkateRuntime::Session&);
    bool Impact(NativeSkateRuntime::Session&, const float velocity[3], float threshold);
    size_t Count() const {
        return objects.size();
    }
};
} // namespace NativeSkateActors

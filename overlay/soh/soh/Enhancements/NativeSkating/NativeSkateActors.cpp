#include "NativeSkateActors.h"
#include "NativeSkateUnits.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
extern "C" {
#include "global.h"
int CollisionCheck_Incompatible(Collider*, Collider*);
}
namespace NativeSkateActors {
namespace {
using P = std::array<float, 3>;
void Triangle(std::vector<float>& v, P a, P b, P c) {
    for (auto p : { a, b, c })
        for (float f : p)
            v.push_back(f);
}
const std::vector<float>& Cylinder() {
    static const auto mesh = []() {
        std::vector<float> v;
        for (int i = 0; i < 12; ++i) {
            float a = i * 6.28318530718f / 12, b = (i + 1) * 6.28318530718f / 12;
            P p = { std::cos(a), 0, std::sin(a) }, q = { std::cos(b), 0, std::sin(b) }, u = p, w = q;
            u[1] = w[1] = 1;
            Triangle(v, p, u, w);
            Triangle(v, p, w, q);
            Triangle(v, { 0, 1, 0 }, w, u);
            Triangle(v, { 0, 0, 0 }, p, q);
        }
        return v;
    }();
    return mesh;
}
const std::vector<float>& Sphere() {
    static const auto mesh = []() {
        std::vector<float> v;
        for (int ring = 0; ring < 6; ++ring)
            for (int i = 0; i < 12; ++i) {
                float a = i * 6.28318530718f / 12, b = (i + 1) * 6.28318530718f / 12,
                      lo = -1.57079632679f + ring * 3.14159265359f / 6, hi = lo + 3.14159265359f / 6;
                P p = { std::cos(lo) * std::cos(a), std::sin(lo), std::cos(lo) * std::sin(a) },
                  q = { std::cos(lo) * std::cos(b), std::sin(lo), std::cos(lo) * std::sin(b) },
                  u = { std::cos(hi) * std::cos(a), std::sin(hi), std::cos(hi) * std::sin(a) },
                  w = { std::cos(hi) * std::cos(b), std::sin(hi), std::cos(hi) * std::sin(b) };
                if (ring < 5)
                    Triangle(v, p, u, w);
                if (ring > 0)
                    Triangle(v, p, w, q);
            }
        return v;
    }();
    return mesh;
}
std::array<float, 16> Frame(float x, float y, float z, float radius, float height) {
    float k = NativeSkateUnits::kOotUnitsPerMeter;
    std::array<float, 16> m{};
    m[0] = m[10] = radius / k;
    m[5] = height / k;
    m[12] = x / k;
    m[13] = y / k;
    m[14] = z / k;
    m[15] = 1;
    return m;
}
} // namespace
void World::Clear(NativeSkateRuntime::Session& r) {
    for (const auto& e : objects)
        if (r.Ready())
            r.Dynamic(e.first, nullptr, e.second.frame.data(), false);
    objects.clear();
    previousContacts.clear();
    registryTick = ~uint64_t(0);
}
void World::ActorInit(const Actor* a, NativeSkateRuntime::Session& r) {
    registryTick = ~uint64_t(0);
    for (auto i = objects.begin(); i != objects.end();)
        if (i->second.actor == a) {
            if (r.Ready())
                r.Dynamic(i->first, nullptr, i->second.frame.data(), false);
            previousContacts.erase(i->first);
            i = objects.erase(i);
        } else
            ++i;
}
bool World::Update(PlayState* play, NativeSkateRuntime::Session& r, uint64_t tick) {
    if (tick == registryTick)
        return true;
    registryTick = tick;
    auto start = std::chrono::steady_clock::now();
    updates = 0;
    for (auto& e : objects)
        e.second.seen = false;
    Player* player = GET_PLAYER(play);
    bool ok = true;
    auto add = [&](Collider* c, int element, const std::vector<float>& local, std::array<float, 16> frame) {
        if (local.empty())
            return;
        for (float f : frame)
            if (!std::isfinite(f))
                return;
        float minx = 1e30f, maxx = -1e30f, minz = 1e30f, maxz = -1e30f;
        for (size_t j = 0; j < local.size(); j += 3) {
            float x = (frame[12] + frame[0] * local[j] + frame[4] * local[j + 1] + frame[8] * local[j + 2]) *
                      NativeSkateUnits::kOotUnitsPerMeter;
            float z = (frame[14] + frame[2] * local[j] + frame[6] * local[j + 1] + frame[10] * local[j + 2]) *
                      NativeSkateUnits::kOotUnitsPerMeter;
            minx = std::min(minx, x);
            maxx = std::max(maxx, x);
            minz = std::min(minz, z);
            maxz = std::max(maxz, z);
        }
        float x = player->actor.world.pos.x, z = player->actor.world.pos.z;
        float dx = x < std::min(minx, maxx) ? minx - x : x > maxx ? x - maxx : 0;
        float dz = z < minz ? minz - z : z > maxz ? z - maxz : 0;
        if (dx * dx + dz * dz > 1200.f * 1200.f)
            return;
        auto found = std::find_if(objects.begin(), objects.end(), [&](const auto& e) {
            return e.second.actor == c->actor && e.second.collider == c && e.second.element == element;
        });
        bool fresh = found == objects.end();
        if (fresh) {
            unsigned id = 256;
            while (objects.count(id) && id < 512)
                ++id;
            if (id == 512)
                return;
            Proxy p;
            p.actor = c->actor;
            p.collider = c;
            p.element = element;
            p.actorId = c->actor->id;
            p.category = c->actor->category;
            p.shape = c->shape;
            p.at = c->atFlags;
            p.ac = c->acFlags;
            p.oc = c->ocFlags1;
            found = objects.emplace(id, std::move(p)).first;
        }
        auto& p = found->second;
        bool geometry = fresh || p.local != local;
        if (geometry || p.frame != frame) {
            if (!r.Dynamic(found->first, geometry ? &local : nullptr, frame.data(), true))
                ok = false;
            ++updates;
        }
        if (!fresh && tick > p.tick) {
            float dt = (tick - p.tick) / 60.f;
            for (int a = 0; a < 3; ++a)
                p.velocity[a] = (frame[12 + a] - p.frame[12 + a]) / dt;
        } else if (fresh)
            std::fill_n(p.velocity, 3, 0.f);
        p.tick = tick;
        p.local = local;
        p.frame = frame;
        p.seen = true;
    };
    auto& ctx = play->colChkCtx;
    for (int i = 0; i < ctx.colOCCount && i < COLLISION_CHECK_OC_MAX; ++i) {
        Collider* c = ctx.colOC[i];
        if (!c || !c->actor || !c->actor->update || c->actor == &player->actor || !(c->ocFlags1 & OC1_ON) ||
            !(c->ocFlags1 & OC1_TYPE_PLAYER) || (c->ocFlags1 & OC1_NO_PUSH))
            continue;
        if (CollisionCheck_Incompatible(c, &player->cylinder.base))
            continue;
        // En_Wood02's tree variants register the same stock solid OC cylinders
        // used by Link. Bush/leaf variants do not qualify; never invent canopy collision.
        const bool tree = c->actor->id == ACTOR_EN_WOOD02 && (c->actor->params & 0xff) <= 10;
        int category = c->actor->category;
        if (category != ACTORCAT_NPC && category != ACTORCAT_ENEMY && category != ACTORCAT_BOSS && !tree)
            continue;
        if (c->shape == COLSHAPE_CYLINDER) {
            auto* s = (ColliderCylinder*)c;
            if (!(s->info.ocElemFlags & OCELEM_ON) || s->dim.radius <= 0 || s->dim.height <= 0)
                continue;
            add(c, 0, Cylinder(),
                Frame(s->dim.pos.x, float(s->dim.pos.y + s->dim.yShift), s->dim.pos.z, s->dim.radius, s->dim.height));
        } else if (c->shape == COLSHAPE_JNTSPH) {
            auto* s = (ColliderJntSph*)c;
            for (int j = 0; j < s->count && j < 16; ++j) {
                auto& e = s->elements[j];
                if (!(e.info.ocElemFlags & OCELEM_ON) || e.dim.worldSphere.radius <= 0)
                    continue;
                auto& b = e.dim.worldSphere;
                add(c, j, Sphere(), Frame(b.center.x, b.center.y, b.center.z, b.radius, b.radius));
            }
        }
        // Stock OC supports only cylinder/joint-sphere pairs. Tris/quad remain
        // stock attack/hurt shapes, rather than being invented permanent obstacles.
    }
    for (auto i = objects.begin(); i != objects.end();)
        if (!i->second.seen) {
            if (!r.Dynamic(i->first, nullptr, i->second.frame.data(), false))
                ok = false;
            previousContacts.erase(i->first);
            i = objects.erase(i);
        } else
            ++i;
    ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    maximum = std::max(maximum, ms);
    total += ms;
    ++samples;
    if (CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ActorCollisionDebug"), 0) && samples % 60 == 0) {
        SPDLOG_INFO("[NativeActors] proxies={} changes={} avg={}ms max={}ms samples={}", objects.size(), updates,
                    total / samples, maximum, samples);
        for (auto& e : objects)
            SPDLOG_INFO("[NativeActors] id={} actor={} category={} class={} shape={} AT={} AC={} OC={} element={}",
                        e.first, e.second.actorId, e.second.category,
                        e.second.category == ACTORCAT_NPC    ? "FRIENDLY"
                        : e.second.category == ACTORCAT_BOSS ? "BOSS"
                                                             : "ENEMY",
                        e.second.shape, e.second.at, e.second.ac, e.second.oc, e.second.element);
    }
    return ok;
}
} // namespace NativeSkateActors

namespace NativeSkateActors {
bool World::Impact(NativeSkateRuntime::Session& r, const float* v, float threshold) {
    if (objects.empty()) {
        previousContacts.clear();
        return false;
    }
    std::vector<std::array<float, 5>> contacts;
    if (!r.ActorContacts(contacts))
        return false;
    std::set<unsigned> current;
    bool result = false;
    for (auto& c : contacts) {
        unsigned id = (unsigned)c[0];
        auto entry = objects.find(id);
        if (entry == objects.end())
            continue;
        current.insert(id);
        bool enemy = entry->second.category == ACTORCAT_ENEMY || entry->second.category == ACTORCAT_BOSS;
        float incoming = -((v[0] - entry->second.velocity[0]) * c[1] + (v[1] - entry->second.velocity[1]) * c[2] +
                           (v[2] - entry->second.velocity[2]) * c[3]);
        if (!previousContacts.count(id) && CVarGetInteger(CVAR_ENHANCEMENT("NativeSkate.ActorCollisionDebug"), 0))
            SPDLOG_INFO(
                "[NativeActors] contact actor={} class={} normal={},{},{} relativeImpact={} nativeTickContact=ONSET",
                entry->second.actorId, enemy ? "ENEMY/BOSS" : "FRIENDLY", c[1], c[2], c[3], incoming);
        if (enemy && !previousContacts.count(id) && incoming >= threshold)
            result = true;
    }
    previousContacts = std::move(current);
    return result;
}
} // namespace NativeSkateActors

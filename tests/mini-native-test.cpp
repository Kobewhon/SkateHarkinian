#include "NativeSkateVert.h"
#include "NativeSkateRuntime.h"
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <random>
#include <chrono>
using namespace NativeSkateVert;
using S = NativeSkateRuntime::Snapshot;
using P = NativeSkateRuntime::Packet;
constexpr float U = 54.f / .885f;
Ramp Make(int type, Id id, float z) {
    auto t = (NativeSkateObjects::Type)type;
    auto& d = NativeSkateObjects::Definitions()[type];
    auto m = NativeSkateObjects::Geometry(t);
    Ramp r;
    r.id = id;
    r.width = d.width;
    r.count = 13;
    r.right = { d.profile == NativeSkateObjects::Profile::Mini ? 1.f : -1.f, 0, 0 };
    for (int j = 0; j < 13; ++j) {
        int source = d.profile == NativeSkateObjects::Profile::Mini ? 12 - j : j;
        auto v = m.vertices[source * 2];
        v.x = 0;
        v.z += z;
        r.points[j] = v;
    }
    return r;
}
int main(int argc, char** argv) {
    assert(argc == 3);
    auto dll = LoadLibraryA(argv[1]);
    assert(dll);
#define API(name, type)                                       \
    auto name = (type)GetProcAddress(dll, "sh_skate_" #name); \
    assert(name)
    using Create = void* (*)(const char*, const float*, uint32_t, const float*, float);
    API(create, Create);
    using Step = int (*)(void*, const P*, uint32_t, S*);
    API(step_samples, Step);
    using Vert = int (*)(void*, const float*, const float*, const float*, uint32_t, float, S*);
    API(vert_step, Vert);
    using Handoff = int (*)(void*, const float*, float, const float*, S*);
    API(handoff, Handoff);
    using Dynamic = int (*)(void*, uint32_t, const float*, uint32_t, const float*, uint32_t);
    API(dynamic, Dynamic);
    using Progress = int (*)(void*, uint64_t*);
    API(worker_progress, Progress);
    using Mods = int (*)(void*, const float*, uint32_t);
    API(modifiers, Mods);
    using Destroy = int (*)(void*);
    API(destroy_checked, Destroy);
    float floor[] = { -100, 0, -100, 100, 0, 100, 100, 0, -100, -100, 0, -100, -100, 0, 100, 100, 0, 100 };
    float spawn[3] = {};
    void* h = create(argv[2], floor, 2, spawn, 0);
    assert(h);
    std::vector<Ramp> ramps;
    for (int type : { 25, 26 }) {
        auto& d = NativeSkateObjects::Definitions()[type];
        float z = (type == 25 ? -1.f : 1.f) * d.length / 2;
        ramps.push_back(Make(type, type, z));
        auto mesh = NativeSkateObjects::Geometry((NativeSkateObjects::Type)type);
        std::vector<float> tri;
        for (auto t : mesh.triangles)
            for (auto i : t) {
                auto p = mesh.vertices[i];
                tri.insert(tri.end(), { p.x / U, p.y / U, p.z / U });
            }
        float frame[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, z / U, 1 };
        assert(dynamic(h, type, tri.data(), mesh.triangles.size(), frame, 3));
    }
    std::mt19937 random(82);
    int commands = 0, enters = 0, exits = 0, plants = 0;
    double maxMs = 0;
    for (int hz : { 60, 120, 144, 240 }) {
        for (int cycle = 0; cycle < 500; ++cycle) {
            auto& r = ramps[cycle % 2];
            auto c = Along(r, (cycle % 7 == 0 ? 1.f : 30.f) + (random() % 70));
            float speed = (cycle % 3 == 0 ? 120.f : cycle % 3 == 1 ? 360.f : 700.f);
            if (cycle % 5 == 0)
                speed = -speed;
            Point board = Add(c.point, Mul(c.normal, 5));
            float position[] = { board.x / U, board.y / U, board.z / U };
            float velocity[] = { c.tangent.x * speed / U, c.tangent.y * speed / U, c.tangent.z * speed / U };
            S snap{};
            assert(handoff(h, position, c.tangent.z < 0 ? 3.14159265f : 0, velocity, &snap));
            Controller controller;
            float modifiersValue[] = { cycle % 11 == 0 ? 2.f : 1.f, cycle % 13 == 0 ? 1.8f : 1.f, 1.f,
                                       cycle % 17 == 0 ? 1.5f : 1.f };
            assert(modifiers(h, modifiersValue, cycle % 9 == 0));
            uint64_t oldTick = snap.tick;
            double accumulator = 0;
            for (int render = 0; render < hz * 2; ++render) {
                accumulator += 1. / hz;
                while (accumulator + 1e-9 >= 1. / 60) {
                    accumulator -= 1. / 60;
                    P packet{};
                    int tick = int(render * 60. / hz);
                    if (tick == 65)
                        packet.buttons = 0x8000;
                    if (tick == 45 || tick == 46)
                        packet.right[1] = tick == 45 ? -32767 : 32767;
                    if (cycle % 4 == 0 && tick > 30 && tick < 80)
                        packet.buttons |= 0x0200;
                    packet.left[0] = (cycle % 3 == 0 ? 0 : int16_t((int(random() % 32000) - 16000)));
                    packet.triggers[0] = cycle % 19 == 0 ? 255 : 0;
                    Point b{ snap.board[12] * U, snap.board[13] * U, snap.board[14] * U };
                    Point v{ snap.trajectoryVelocity[0] * U, snap.trajectoryVelocity[1] * U,
                             snap.trajectoryVelocity[2] * U };
                    bool was = controller.Active();
                    bool request = packet.buttons & 0x8000 || std::abs((int)packet.right[1]) > 8000;
                    bool eligible = was || CanAcquire(snap.stateId, false, snap.stateId >= 200 && snap.stateId <= 202);
                    bool now = controller.Update(ramps, b, v, eligible, (packet.buttons & 0x0200) != 0, 1.f / 60,
                                                 request, float(packet.left[0]) / 32767);
                    if (now && !was)
                        ++enters;
                    if (was && !now)
                        ++exits;
                    if (controller.award)
                        ++plants;
                    float f[16], vel[3], a[3];
                    controller.Frame(1 / U, f);
                    for (int i = 0; i < 3; ++i) {
                        vel[i] = (&controller.velocity.x)[i] / U;
                        a[i] = (&controller.anchor.x)[i] / U;
                    }
                    auto begin = std::chrono::steady_clock::now();
                    if (now)
                        assert(vert_step(h, f, vel, a, controller.Handplant() ? 2 : 1, controller.Weight(), &snap));
                    else {
                        if (was)
                            assert(vert_step(h, f, vel, a, 0, 0, &snap));
                        assert(step_samples(h, &packet, 1, &snap));
                    }
                    double ms =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
                    maxMs = std::max(maxMs, ms);
                    assert(ms < 2000);
                    ++commands;
                    assert(snap.tick > oldTick);
                    oldTick = snap.tick;
                    for (float q : snap.board)
                        assert(std::isfinite(q));
                    for (float q : snap.root)
                        assert(std::isfinite(q));
                    assert(controller.Active() ? controller.owner != 0 : controller.owner == 0);
                    if (request)
                        assert(!controller.Active());
                    uint64_t heart[6];
                    assert(worker_progress(h, heart));
                    assert(heart[0] - heart[1] <= 1);
                }
            }
            // F8 legal handoff invariant: no activation-position reset; controller teardown
            // leaves the final rider position intact. F9 canonical reseed of native state.
            auto final = snap;
            controller.Reset();
            for (int i = 0; i < 3; ++i)
                assert(snap.board[12 + i] == final.board[12 + i]);
            assert(handoff(h, spawn, 0, spawn, &snap));
            assert(snap.stateId < 1000);
        }
        printf(
            "PASS mini native %d FPS:500 cycles geometry25/26 paired, low/medium/high, alternating walls/carve/pop/trigger/dismount; ticks/finite ownership/commands\n",
            hz);
        fflush(stdout);
    }
    assert(destroy_checked(h));
    printf("PASS mini native2000cycles commands=%d enters=%d exits=%d handplants=%d maxCommandMs=%.3f\n", commands,
           enters, exits, plants, maxMs);
}
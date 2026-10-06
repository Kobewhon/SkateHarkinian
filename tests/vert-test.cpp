#include "NativeSkateVert.h"
#include <cassert>
#include <cstdio>
using namespace NativeSkateVert;
Ramp Make(float yaw = 0) {
    Ramp r;
    r.id = 42;
    r.width = 200;
    r.count = 13;
    Pose p{};
    p.yaw = yaw;
    r.right = NativeSkateObjectEditor::Rotate({ -1, 0, 0 }, p);
    for (int i = 0; i < 13; ++i) {
        float f = i / 12.f;
        r.points[i] = NativeSkateObjectEditor::Rotate({ 0, 100 * (1 - std::sqrt(1 - f * f)), 200 * f }, p);
    }
    return r;
}
int main() {
    assert(CanAcquire(100, false, false));
    assert(CanAcquire(201, false, true));
    assert(!CanAcquire(201, false, false));
    assert(!CanAcquire(100, true, true));
    assert(!CanAcquire(401, false, true));
    assert(!CanAcquire(600, false, true));
    assert(!CanAcquire(500, false, true));
    for (int hz : { 60, 120, 144, 240 })
        for (float yaw : { 0.f, .785398f, 1.570796f, 3.141593f, 4.712389f }) {
            auto r = Make(yaw);
            std::vector<Ramp> ramps{ r };
            auto first = Along(r, 50);
            Controller c;
            Point velocity = Mul(first.tangent, 600);
            assert(c.Update(ramps, first.point, velocity, true, false, 1.f / hz));
            bool steep = false, air = false;
            for (int i = 0; i < hz * 4; ++i) {
                float frame[16];
                c.Frame(1 / 61.f, frame);
                for (float x : frame)
                    assert(std::isfinite(x));
                if (std::abs(c.forward.y) > .75f)
                    steep = true;
                if (!c.Update(ramps, c.point, c.velocity, true, false, 1.f / hz)) {
                    air = c.exit;
                    break;
                }
            }
            assert(steep && air);
            Controller low;
            assert(low.Update(ramps, first.point, Mul(first.tangent, 100), true, false, 1.f / hz));
            bool rollback = false;
            for (int i = 0; i < hz * 4; ++i) {
                if (low.speed < 0)
                    rollback = true;
                if (!low.Update(ramps, low.point, low.velocity, true, false, 1.f / hz))
                    break;
            }
            assert(rollback);
            auto lip = Along(r, Along(r, 10000).length - 10);
            Controller plant;
            assert(plant.Update(ramps, lip.point, Mul(lip.tangent, 150), true, false, 1.f / hz));
            assert(plant.Update(ramps, plant.point, plant.velocity, true, true, 1.f / hz) && plant.award &&
                   plant.Handplant());
            auto anchor = plant.anchor;
            int awards = 1;
            for (int i = 0; i < hz * 2; ++i) {
                auto previous = plant.point;
                bool hadPlant = plant.Handplant();
                plant.Update(ramps, plant.point, plant.velocity, true, i < hz / 2, 1.f / hz);
                if (hadPlant && plant.Handplant())
                    assert(Length(Sub(plant.point, previous)) < 1800.f / hz);
                if (plant.award)
                    ++awards;
                if (plant.Handplant())
                    assert(Length(Sub(plant.anchor, anchor)) < .001f);
            }
            assert(awards == 1 && !plant.Handplant());
            plant.Reset();
            assert(!plant.Active());
            Controller flat;
            auto bottom = Along(r, 0);
            assert(!flat.Update(ramps, bottom.point, {}, true, false, 1.f / hz));
            Controller stale;
            stale.owner = 42;
            stale.distance = 50;
            assert(!stale.Update(ramps, { 10000, 10000, 10000 }, {}, true, false, 1.f / hz) && stale.owner == 0);
            Controller exitInput;
            assert(exitInput.Update(ramps, first.point, velocity, true, false, 1.f / hz));
            assert(!exitInput.Update(ramps, exitInput.point, exitInput.velocity, true, false, 1.f / hz, true) &&
                   exitInput.owner == 0);
            Controller walls;
            assert(!walls.Update({}, first.point, velocity, true, true, 1.f / hz));
            Controller off;
            assert(!off.Update(ramps, first.point, velocity, false, true, 1.f / hz));
            Controller deleted;
            deleted.Update(ramps, first.point, velocity, true, false, 1.f / hz);
            assert(!deleted.Update({}, deleted.point, deleted.velocity, true, false, 1.f / hz) && deleted.exit);
        }
    puts(
        "PASS registered quarter transition ascent/near-vertical/air-out, low-speed rollback, coping anchor/one-shot handplant/release, delete/F9 reset, wall/offboard negatives across60/120/144/240 and five yaw angles");
}

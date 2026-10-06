#include "NativeSkateHudState.h"
#include <cassert>
#include <cstdio>
using namespace NativeSkateHud;
int main() {
    for (int hz : { 60, 120, 144, 240 }) {
        Line l;
        l.Start(0);
        l.Observe(100);
        assert(l.Display() == 100);
        l.Observe(20);
        assert(l.Display() == 100);
        l.Commit(0, 60);
        assert(l.Display() == 100);
        l.Advance(210);
        assert(l.active);
        l.Advance(211);
        assert(!l.active && l.total == 100);
        l.Advance(1000);
        assert(l.total == 100);
        for (int eventTick : { 60, 120, 149 }) {
            Line x;
            x.Start(0);
            x.Commit(100, 0);
            x.Advance(eventTick);
            assert(x.active);
            x.Start(eventTick);
            x.Observe(25);
            x.Commit(5, eventTick);
            assert(x.score == 125);
            x.Advance(eventTick + 150);
            assert(x.active);
            x.Advance(eventTick + 151);
            assert(x.total == 125);
        }
        for (const char* action : { "GRIND", "MANUAL", "GRAB", "HANDPLANT", "AIR" }) {
            (void)action;
            Line x;
            x.Start(0);
            x.Commit(100, 0);
            for (int f = 0; f < hz * 10; ++f)
                x.Advance((uint64_t)(f * 60. / hz), true);
            assert(x.active);
            x.Advance(600, true);
            x.Advance(750);
            assert(x.active);
            x.Advance(751);
            assert(x.total == 100);
        }
    }
    Line l;
    uint64_t time = 0;
    for (int line = 0; line < 1000; ++line) {
        uint64_t prev = 0;
        for (int trick = 0; trick < 10; ++trick) {
            l.Start(time);
            l.Observe(100);
            l.Observe(20);
            assert(l.Display() >= prev);
            prev = l.Display();
            l.Commit(50, time);
            assert(l.Display() >= prev);
            prev = l.Display();
            time += 60;
        }
        auto total = l.total;
        l.Advance(time + 151);
        assert(l.total == total + 1000);
        l.Advance(time + 1000);
        assert(l.total == total + 1000);
        time += 1200;
    }
    assert(l.total == 1000000);
    l.Start(time);
    for (int i = 0; i < 500; ++i) {
        auto prev = l.Display();
        l.Observe(float(i * 20));
        assert(l.Display() >= prev);
    }
    l.Exit(time);
    auto bank = l.total;
    l.Exit(time);
    assert(l.total == bank);
    l.Cancel();
    assert(!l.active && l.total == bank);
    State s;
    NativeSkateRuntime::Event e{};
    e.sequence = 1;
    e.tick = 1;
    e.phase = 1;
    e.id = 1;
    e.combo = 100;
    e.current = 100;
    e.multiplier = 1;
    snprintf(e.name, sizeof(e.name), "OLLIE");
    s.Apply(e);
    e.sequence = 2;
    e.tick = 2;
    e.combo = 20;
    s.Apply(e);
    assert(s.line.Display() == 100);
    e.sequence = 3;
    e.tick = 1;
    e.combo = 900;
    s.Apply(e);
    assert(s.line.Display() == 100);
    e.sequence = 3;
    e.tick = 3;
    e.phase = 4;
    e.combo = 20;
    e.current = 0;
    s.Apply(e);
    assert(s.line.score == 100);
    s.Apply(e);
    assert(s.line.score == 100);
    assert(AddPoints(UINT64_MAX - 1, 100) == UINT64_MAX);
    puts(
        "PASS authoritative monotonic LINE, exact >2.50-second bank, continuous actions, stale/duplicate events, 10,000 tricks/1,000 LINEs, long LINE, F8/F9/bail-safe cancellation and saturation at60/120/144/240");
}

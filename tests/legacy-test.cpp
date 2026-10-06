#include "NativeSkateLegacyPop.h"
#include <cassert>
#include <cstdio>
#include <random>
using NativeSkateLegacyPop::State;
void prime(State& s) {
    assert(!s.Read(true, 1, false, false, 0, false, false, 100, 0));
    assert(!s.Read(true, 1, true, true, 0, false, false, 100, .1));
}
int main() {
    for (int fps : { 60, 120, 144, 240 }) {
        for (int i = 0; i < 500; ++i) {
            State s;
            prime(s);
            assert(!s.Read(true, 1, true, true, 1, false, false, 100, .2));
            assert(s.Read(true, 1, true, true, 1, true, true, .1, .3));
            for (int k = 0; k < fps; ++k)
                assert(!s.Read(true, 1, true, true, 1, true, true, .1, .3 + double(k) / fps));
            assert(s.consumed);
        }
        for (int i = 0; i < 500; ++i) {
            State s;
            prime(s);
            assert(!s.Read(false, 1, true, true, 1, false, false, 100, .2));
            assert(!s.Read(false, 1, true, true, 1, true, true, .1, .3));
        }
        std::mt19937 rng(82);
        for (int i = 0; i < 500; ++i) {
            State s;
            prime(s);
            bool triggerFirst = (rng() % 2) != 0, landing = (rng() % 2) != 0;
            double dt = double(rng() % 40) / 100;
            bool first = s.Read(true, 1, true, true, triggerFirst ? 1 : 0, !triggerFirst, false, 100, .2);
            assert(!first);
            bool out = s.Read(true, 1, true, true, 1, true, landing, dt, .3);
            assert(out == (triggerFirst && landing && dt <= .16));
            assert(!s.Read(true, 2, true, true, 1, true, true, .1, .4));
        }
        State wrong;
        assert(!wrong.Read(true, 1, false, false, 0, false, false, 100, 0));
        assert(!wrong.Read(true, 1, false, true, 1, false, false, 100, .2));
        assert(!wrong.Read(true, 1, false, true, 1, true, true, .1, .3));
        State both;
        prime(both);
        assert(!both.Read(true, 1, true, true, 1, false, false, 100, .2));
        assert(!both.Read(true, 1, true, true, 3, false, false, 100, .21));
        assert(both.Read(true, 1, true, true, 3, true, true, .1, .3));
    }
    puts(
        "PASS legacy ordered-input state: each FPS500 valid/500 disabled/500 randomized;wrong trick,scene-generation discard,second trigger edge,holding one-shot");
}

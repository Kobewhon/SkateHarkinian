#pragma once
#include "NativeSkateRuntime.h"
#include "NativeSkateObjectRegistry.h"
#include <algorithm>
#include <cmath>
namespace NativeSkateAudio {
using Surface = NativeSkateObjects::AudioSurface;
enum class Cue {
    Push,
    Pop,
    LandSoft,
    LandMedium,
    LandHard,
    GrindStart,
    GrindEnd,
    SlideStart,
    SlideEnd,
    BailBoard,
    BailBody,
    Pickup,
    Drop,
    Mount,
    Dismount
};
struct Frame {
    uint64_t tick = 0;
    uint32_t state = 700;
    float speed = 0, vertical = 0;
    bool held = false, push = false, pop = false, water = false;
    Surface surface = Surface::Generic;
};
struct State {
    bool valid = false, popLaunching = false;
    uint64_t popIntentUntil = 0;
    Frame previous{};
    float impact = 0;
    uint64_t lastPush = 0, lastPop = 0;
    float roll = 0, grind = 0, slide = 0;
    void Reset() {
        *this = {};
    }
    template <class Emit, class Loop> void Read(const Frame& f, Emit emit, Loop loop) {
        if (valid && f.tick <= previous.tick)
            return;
        float dt = valid ? std::min(.1f, (f.tick - previous.tick) / 60.f) : 1 / 60.f;
        bool biped = f.state == 500 || f.state == 501;
        bool ground = f.state == 100 || f.state == 101 || f.state == 102 || f.state == 103 || f.state == 1000;
        if (f.pop)
            popLaunching = true;
        if (f.state != 103 && !(f.state >= 200 && f.state < 300))
            popLaunching = false;
        ground = ground && !popLaunching;
        bool grinding = f.state >= 400 && f.state <= 405;
        bool sliding = f.state == 101 || f.state == 400 || f.state == 402 || f.state == 405;
        auto one = [&](Cue c) { emit(c, f.surface); };
        if (valid) {
            bool oldBiped = previous.state == 500 || previous.state == 501,
                 oldGrind = previous.state >= 400 && previous.state <= 405,
                 oldSlide =
                     previous.state == 101 || previous.state == 400 || previous.state == 402 || previous.state == 405;
            if (biped && !oldBiped)
                one(Cue::Dismount);
            if (!biped && oldBiped && f.state != 300)
                one(Cue::Mount);
            if (f.held && !previous.held)
                one(Cue::Pickup);
            if (!f.held && previous.held)
                one(Cue::Drop);
            if (grinding && !oldGrind && !sliding)
                one(Cue::GrindStart);
            if (oldGrind && !grinding && !oldSlide)
                one(Cue::GrindEnd);
            if (sliding && !oldSlide)
                one(Cue::SlideStart);
            if (oldSlide && !sliding)
                one(Cue::SlideEnd);
            if (f.state == 300 && previous.state != 300) {
                if (!f.water) {
                    one(Cue::BailBoard);
                    one(Cue::BailBody);
                } else
                    one(Cue::Drop);
            }
            if (ground && previous.state >= 200 && previous.state < 300) {
                one(impact < 2 ? Cue::LandSoft : impact < 5 ? Cue::LandMedium : Cue::LandHard);
                impact = 0;
            }
        }
        if (f.state >= 200 && f.state < 300)
            impact = std::max(impact, std::max(0.f, -f.vertical));
        if (f.push && (!valid || !previous.push) && f.tick != lastPush) {
            one(Cue::Push);
            lastPush = f.tick;
        }
        if (f.pop && (!valid || !previous.pop) && f.tick != lastPop) {
            one(Cue::Pop);
            lastPop = f.tick;
        }
        float intensity = std::clamp(f.speed / 12.f, 0.f, 1.f);
        auto fade = [&](float& v, float target) { v += std::clamp(target - v, -dt * 8, dt * 8); };
        fade(roll, ground && !biped && f.speed > .15f && !sliding ? (.1f + .5f * intensity) : 0);
        fade(grind, grinding && !sliding ? .15f + .55f * intensity : 0);
        fade(slide, sliding ? .12f + .5f * intensity : 0);
        loop(0, f.surface, roll, .85f + .3f * intensity);
        loop(1, Surface::Metal, grind, .9f + .2f * intensity);
        loop(2, f.surface, slide, .9f + .2f * intensity);
        previous = f;
        valid = true;
    }
};
} // namespace NativeSkateAudio

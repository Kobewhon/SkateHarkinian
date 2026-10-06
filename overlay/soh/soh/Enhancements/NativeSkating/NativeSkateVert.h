#pragma once
#include "NativeSkateObjectEditor.h"
#include <array>
#include <vector>
namespace NativeSkateVert {
using NativeSkateObjects::Point;
using NativeSkateObjects::Pose;
using NativeSkateObjects::Id;
inline Point Add(Point a, Point b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}
inline Point Sub(Point a, Point b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
inline Point Mul(Point p, float f) {
    return { p.x * f, p.y * f, p.z * f };
}
inline float Dot(Point a, Point b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline float Length(Point p) {
    return std::sqrt(Dot(p, p));
}
inline Point Unit(Point p) {
    float n = Length(p);
    return n > 1e-5f ? Mul(p, 1 / n) : Point{ 0, 1, 0 };
}
inline Point Cross(Point a, Point b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
struct Ramp {
    Id id = 0;
    float width = 0;
    Point right{ -1, 0, 0 }, lipTangent{ 0, 1, 0 };
    std::array<Point, 16> points{};
    int count = 0;
};
struct Contact {
    Point point{}, tangent{}, normal{};
    float distance = 0, length = 0;
};
inline Contact Along(const Ramp& r, float s) {
    Contact c;
    for (int i = 1; i < r.count; ++i)
        c.length += Length(Sub(r.points[i], r.points[i - 1]));
    s = std::clamp(s, 0.f, c.length);
    c.distance = s;
    for (int i = 1; i < r.count; ++i) {
        auto delta = Sub(r.points[i], r.points[i - 1]);
        float n = Length(delta);
        if (s <= n || i == r.count - 1) {
            c.point = Add(r.points[i - 1], Mul(delta, n > 0 ? s / n : 0));
            float t = n > 0 ? s / n : 0;
            Point begin = Unit(delta), end = i == r.count - 1
                                                 ? r.lipTangent
                                                 : Unit(Add(Unit(delta), Unit(Sub(r.points[i + 1], r.points[i]))));
            if (i > 1)
                begin = Unit(Add(Unit(Sub(r.points[i - 1], r.points[i - 2])), Unit(delta)));
            c.tangent = Unit(Add(Mul(begin, 1 - t), Mul(end, t)));
            c.normal = Unit(Cross(r.right, c.tangent));
            return c;
        }
        s -= n;
    }
    return c;
}
enum class Phase { None, Transition, Enter, Plant, Hold, Release };
// Explicit transition acquisition never steals an existing grind/world plant
// or freezes an uncaught airborne board at an arbitrary flip orientation.
inline bool CanAcquire(unsigned state, bool boardGrab, bool footOnDeck) {
    return !boardGrab && ((state >= 100 && state <= 105) || ((state >= 200 && state <= 202) && footOnDeck));
}
struct Controller {
    Id owner = 0;
    Phase phase = Phase::None;
    float distance = 0, speed = 0, lateral = 0, time = 0, facing = 1;
    bool rWasHeld = false, award = false, exit = false;
    Point anchor{}, point{}, up{ 0, 1, 0 }, forward{ 0, 0, 1 }, velocity{};
    static float Smooth(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3 - 2 * t);
    }
    float Weight() const {
        return phase == Phase::Enter     ? Smooth(time / .18f)
               : phase == Phase::Release ? 1 - Smooth(time / .20f)
               : Handplant()             ? 1.f
                                         : 0.f;
    }
    void Reset() {
        *this = {};
    }
    bool Active() const {
        return phase != Phase::None;
    }
    bool Handplant() const {
        return phase != Phase::None && phase != Phase::Transition;
    }
    bool Update(const std::vector<Ramp>& ramps, Point board, Point v, bool riding, bool rHeld, float dt,
                bool releaseInput = false, float steer = 0) {
        award = exit = false;
        bool fresh = rHeld && !rWasHeld;
        rWasHeld = rHeld;
        if (!riding) {
            if (Active())
                exit = true;
            phase = Phase::None;
            owner = 0;
            return false;
        }
        const Ramp* ramp = nullptr;
        if (Active())
            for (const auto& r : ramps)
                if (r.id == owner)
                    ramp = &r;
        if (!Active()) {
            owner = 0;
            if (releaseInput)
                return false;
            float best = 18;
            for (const auto& r : ramps) {
                float run = 0;
                for (int i = 1; i < r.count; ++i) {
                    Point delta = Sub(r.points[i], r.points[i - 1]);
                    float n = Length(delta);
                    if (n < .01f)
                        continue;
                    float t = std::clamp(Dot(Sub(board, r.points[i - 1]), delta) / (n * n), 0.f, 1.f);
                    auto c = Along(r, run + t * n);
                    Point right = Unit(Cross(c.tangent, c.normal));
                    float side = Dot(Sub(board, c.point), right);
                    Point nearest = Add(c.point, Mul(right, side));
                    float gap = Length(Sub(board, nearest));
                    if (c.tangent.y > .12f && Dot(Sub(board, nearest), c.normal) > -2 &&
                        std::abs(side) < r.width * .5f - 5 && gap < best && Dot(v, c.normal) < 100) {
                        best = gap;
                        owner = r.id;
                        distance = c.distance;
                        lateral = side;
                        speed = Dot(v, c.tangent);
                        ramp = &r;
                    }
                    run += n;
                }
            }
            if (!ramp)
                return false;
            facing = speed < 0 ? -1.f : 1.f;
            phase = Phase::Transition;
        }
        if (!ramp) {
            phase = Phase::None;
            owner = 0;
            exit = true;
            return false;
        }
        auto c = Along(*ramp, distance);
        auto right = Unit(Cross(c.tangent, c.normal));
        // Leaving ownership preserves this contact frame for one native handoff.
        // Never retain a provider ID in None: it previously bypassed acquisition.
        auto leave = [&]() {
            point = Add(Add(c.point, Mul(right, lateral)), Mul(c.normal, 5));
            up = c.normal;
            forward = Mul(c.tangent, facing);
            velocity = Mul(c.tangent, speed);
            phase = Phase::None;
            owner = 0;
            exit = true;
            return false;
        };
        if (releaseInput)
            return leave();
        if (phase == Phase::Transition) {
            lateral += std::clamp(steer, -1.f, 1.f) * std::abs(speed) * .6f * dt;
            if (std::abs(lateral) >= ramp->width * .5f - 5)
                return leave();
        }
        if (phase == Phase::Transition) {
            if (fresh && c.length - distance < 28 && speed > 25) {
                phase = Phase::Enter;
                time = 0;
                award = true;
                anchor = Add(Along(*ramp, c.length).point, Mul(right, lateral));
            } else {
                speed -= (9.81f * 54.f / .885f) * c.tangent.y * dt;
                speed *= std::pow(.997f, dt * 60);
                distance += speed * dt;
                if (distance < 0) {
                    c = Along(*ramp, 0);
                    point = Add(Add(c.point, Mul(right, lateral)), Mul(c.normal, 5));
                    up = c.normal;
                    forward = Mul(c.tangent, facing);
                    velocity = Mul(c.tangent, speed);
                    phase = Phase::None;
                    owner = 0;
                    exit = true;
                    return false;
                }
                if (distance >= c.length) {
                    distance = c.length;
                    if (speed > 250) {
                        c = Along(*ramp, c.length);
                        point = Add(Add(c.point, Mul(right, lateral)), Mul(c.normal, 5));
                        up = c.normal;
                        forward = c.tangent;
                        velocity = Mul(c.tangent, speed);
                        phase = Phase::None;
                        owner = 0;
                        exit = true;
                        return false;
                    }
                    speed = -std::max(40.f, std::abs(speed) * .6f);
                }
            }
        }
        c = Along(*ramp, distance);
        right = Unit(Cross(c.tangent, c.normal));
        point = Add(Add(c.point, Mul(right, lateral)), Mul(c.normal, 5));
        up = c.normal;
        forward = Mul(c.tangent, facing);
        velocity = Mul(c.tangent, speed);
        if (Handplant()) {
            time += dt;
            float angle = 0;
            if (phase == Phase::Enter) {
                angle = Smooth(time / .18f) * 2.65f;
                if (time >= .18f) {
                    phase = Phase::Plant;
                    time = 0;
                }
            } else if (phase == Phase::Plant) {
                angle = 2.65f;
                if (time >= .08f) {
                    phase = Phase::Hold;
                    time = 0;
                }
            } else if (phase == Phase::Hold) {
                angle = 2.65f + .04f * std::sin(time * 7.f);
                if (!rHeld || time >= .65f) {
                    phase = Phase::Release;
                    time = 0;
                }
            } else if (phase == Phase::Release) {
                angle = 2.65f * (1 - Smooth(time / .20f));
                if (time >= .20f) {
                    phase = Phase::Transition;
                    distance = std::max(0.f, c.length - 8);
                    speed = -std::max(120.f, std::abs(speed) * .65f);
                    time = 0;
                }
            }
            if (!Handplant()) {
                c = Along(*ramp, distance);
                point = Add(Add(c.point, Mul(right, lateral)), Mul(c.normal, 5));
                up = c.normal;
                forward = Mul(c.tangent, facing);
                velocity = Mul(c.tangent, speed);
                return true;
            }
            float a = std::cos(angle), b = std::sin(angle);
            up = Add(Mul(c.normal, a), Mul(c.tangent, b));
            forward = Sub(Mul(c.tangent, a), Mul(c.normal, b));
            // Board/body remain one rigid frame; planted hand is solved to anchor by IK.
            Point planted = Add(anchor, Add(Mul(up, 52), Mul(forward, -20)));
            auto releaseContact = Along(*ramp, std::max(0.f, c.length - 8));
            Point resting = phase == Phase::Release
                                ? Add(Add(releaseContact.point, Mul(right, lateral)), Mul(releaseContact.normal, 5))
                                : point;
            point = Add(Mul(resting, 1 - Weight()), Mul(planted, Weight()));
            velocity = {};
        }
        return true;
    }
    void Frame(float metersPerUnit, float out[16]) const {
        Point z = Unit(forward), y = Unit(up), x = Unit(Cross(y, z));
        y = Unit(Cross(z, x));
        std::fill_n(out, 16, 0.f);
        for (int i = 0; i < 3; ++i) {
            out[i] = i == 0 ? x.x : i == 1 ? x.y : x.z;
            out[4 + i] = i == 0 ? y.x : i == 1 ? y.y : y.z;
            out[8 + i] = i == 0 ? z.x : i == 1 ? z.y : z.z;
            out[12 + i] = (i == 0 ? point.x : i == 1 ? point.y : point.z) * metersPerUnit;
        }
        out[15] = 1;
    }
};
} // namespace NativeSkateVert

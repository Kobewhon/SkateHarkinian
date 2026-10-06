#include "NativeSkateGeometry.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
namespace NativeSkateGeometry {
namespace {
Point Sub(Point a, Point b) {
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}
Point Cross(Point a, Point b) {
    return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
float Length(Point a) {
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}
Point Normal(const Triangle& t) {
    auto n = Cross(Sub(t.b, t.a), Sub(t.c, t.a));
    float l = Length(n);
    if (l > 1e-8f)
        for (auto& v : n)
            v /= l;
    return n;
}
bool Valid(const Triangle& t) {
    for (auto p : { t.a, t.b, t.c })
        for (float f : p)
            if (!std::isfinite(f) || std::abs(f) > 20000)
                return false;
    return Length(Cross(Sub(t.b, t.a), Sub(t.c, t.a))) > 1e-8f;
}
struct Riser {
    int axis;
    float at, lo, hi, bottom, top;
    std::vector<size_t> ids;
};
} // namespace
Surface Classify(const Triangle& t) {
    auto n = Normal(t);
    float shortest = std::min({ Length(Sub(t.a, t.b)), Length(Sub(t.a, t.c)), Length(Sub(t.b, t.c)) });
    if (shortest < 0.005f)
        return Surface::Micro;
    if (n[1] < -0.05f)
        return Surface::TooSteep;
    float y = std::abs(n[1]);
    if (y > .985f)
        return Surface::Flat;
    if (y > .5f)
        return Surface::Bank;
    if (y > .08f)
        return Surface::TooSteep;
    float height = std::max({ t.a[1], t.b[1], t.c[1] }) - std::min({ t.a[1], t.b[1], t.c[1] });
    return height <= .03f ? Surface::Curb : Surface::Wall;
}
bool Floor(const std::vector<Triangle>& ts, const Point& p, float& height, Point& normal) {
    bool found = false;
    height = -1e20f;
    for (const auto& t : ts) {
        auto n = Normal(t);
        if (n[1] < .2f)
            continue;
        float den = (t.b[2] - t.c[2]) * (t.a[0] - t.c[0]) + (t.c[0] - t.b[0]) * (t.a[2] - t.c[2]);
        if (std::abs(den) < 1e-8f)
            continue;
        float u = ((t.b[2] - t.c[2]) * (p[0] - t.c[0]) + (t.c[0] - t.b[0]) * (p[2] - t.c[2])) / den;
        float v = ((t.c[2] - t.a[2]) * (p[0] - t.c[0]) + (t.a[0] - t.c[0]) * (p[2] - t.c[2])) / den;
        float w = 1 - u - v;
        if (std::min({ u, v, w }) < -.0001f)
            continue;
        float y = u * t.a[1] + v * t.b[1] + w * t.c[1];
        if (y <= p[1] + .35f && y > height) {
            height = y;
            normal = n;
            found = true;
        }
    }
    return found;
}
void Plane(Result& r, Point center, float yaw, Point dim, float rise, Surface type) {
    float co = std::cos(yaw), si = std::sin(yaw);
    auto point = [&](float x, float z, float y) {
        return Point{ center[0] + x * co + z * si, center[1] + y, center[2] - x * si + z * co };
    };
    auto a = point(-dim[0] / 2, -dim[2] / 2, 0), b = point(dim[0] / 2, -dim[2] / 2, 0),
         c = point(dim[0] / 2, dim[2] / 2, rise), d = point(-dim[0] / 2, dim[2] / 2, rise);
    Triangle x{ a, c, b, type, true }, y{ a, d, c, type, true };
    if (Valid(x) && Valid(y)) {
        r.triangles.push_back(x);
        r.triangles.push_back(y);
    }
}
Result Adapt(const std::vector<float>& input, const Options& options, bool enabled) {
    Result r;
    for (size_t i = 0; i + 8 < input.size(); i += 9) {
        Triangle t{ { input[i], input[i + 1], input[i + 2] },
                    { input[i + 3], input[i + 4], input[i + 5] },
                    { input[i + 6], input[i + 7], input[i + 8] } };
        if (!Valid(t)) {
            ++r.rejected;
            continue;
        }
        t.surface = Classify(t);
        r.triangles.push_back(t);
    }
    if (!enabled)
        return r;
    // Merge two-triangle axis-aligned riser rectangles by their geometric plane.
    // Then require repeated height/run/width and a connected elevation chain.
    std::map<std::tuple<int, int, int, int, int, int>, Riser> map;
    const size_t originalCount = r.triangles.size();
    for (size_t i = 0; i < originalCount; ++i) {
        auto& t = r.triangles[i];
        if (t.surface != Surface::Wall && t.surface != Surface::Curb)
            continue;
        Point lo = t.a, hi = t.a;
        for (auto p : { t.b, t.c })
            for (int k = 0; k < 3; ++k) {
                lo[k] = std::min(lo[k], p[k]);
                hi[k] = std::max(hi[k], p[k]);
            }
        int axis = hi[0] - lo[0] < .001f ? 0 : hi[2] - lo[2] < .001f ? 2 : -1;
        if (axis < 0)
            continue;
        int side = 2 - axis;
        float h = hi[1] - lo[1], width = hi[side] - lo[side];
        if (h < .005f || h > .35f || width < .4f)
            continue;
        auto q = [](float f) { return (int)std::lround(f * 1000); };
        auto key = std::make_tuple(axis, q(lo[axis]), q(lo[side]), q(hi[side]), q(lo[1]), q(hi[1]));
        auto& v = map[key];
        v = { axis, lo[axis], lo[side], hi[side], lo[1], hi[1], v.ids };
        v.ids.push_back(i);
    }
    std::vector<Riser> risers;
    for (auto& entry : map)
        if (entry.second.ids.size() == 2) {
            auto v = entry.second;
            const auto& a = r.triangles[v.ids[0]];
            const auto& b = r.triangles[v.ids[1]];
            std::vector<Point> unique;
            for (auto p : { a.a, a.b, a.c, b.a, b.b, b.c })
                if (std::find(unique.begin(), unique.end(), p) == unique.end())
                    unique.push_back(p);
            float area =
                (Length(Cross(Sub(a.b, a.a), Sub(a.c, a.a))) + Length(Cross(Sub(b.b, b.a), Sub(b.c, b.a)))) * .5f;
            if (unique.size() == 4 && std::abs(area - (v.hi - v.lo) * (v.top - v.bottom)) < .001f)
                risers.push_back(v);
        }
    std::vector<bool> consumed(risers.size()), remove(originalCount);
    for (size_t i = 0; i < risers.size(); ++i) {
        if (consumed[i])
            continue;
        std::vector<size_t> chain{ i };
        auto current = risers[i];
        float step = current.top - current.bottom;
        int direction = 0;
        while (options.stairs) {
            size_t next = risers.size();
            for (size_t j = 0; j < risers.size(); ++j) {
                auto b = risers[j];
                float run = b.at - current.at;
                if (consumed[j] || std::find(chain.begin(), chain.end(), j) != chain.end() || b.axis != current.axis ||
                    std::abs(b.lo - current.lo) > .025f || std::abs(b.hi - current.hi) > .025f ||
                    std::abs(b.bottom - current.top) > .005f || std::abs((b.top - b.bottom) - step) > .02f ||
                    std::abs(run) < .08f || std::abs(run) > .7f || (direction && run * direction < 0))
                    continue;
                if (chain.size() > 1) {
                    float old = current.at - risers[chain[chain.size() - 2]].at;
                    if (std::abs(std::abs(run) - std::abs(old)) > .05f)
                        continue;
                }
                if (next == risers.size() || std::abs(run) < std::abs(risers[next].at - current.at))
                    next = j;
            }
            if (next == risers.size())
                break;
            direction = risers[next].at > current.at ? 1 : -1;
            chain.push_back(next);
            current = risers[next];
        }
        bool connected = true;
        for (size_t k = 0; k + 1 < chain.size(); ++k) {
            auto a = risers[chain[k]], b = risers[chain[k + 1]];
            int side = 2 - a.axis;
            float area = 0;
            for (size_t t = 0; t < originalCount; ++t) {
                auto& tri = r.triangles[t];
                if (tri.surface != Surface::Flat)
                    continue;
                bool inside = true;
                for (auto p : { tri.a, tri.b, tri.c })
                    if (std::abs(p[1] - a.top) > .005f || p[a.axis] < std::min(a.at, b.at) - .001f ||
                        p[a.axis] > std::max(a.at, b.at) + .001f || p[side] < a.lo - .001f || p[side] > a.hi + .001f)
                        inside = false;
                if (inside)
                    area += Length(Cross(Sub(tri.b, tri.a), Sub(tri.c, tri.a))) * .5f;
            }
            if (area < (a.hi - a.lo) * std::abs(b.at - a.at) * .8f)
                connected = false;
        }
        if (chain.size() >= 3 && connected) {
            auto first = risers[i], last = risers[chain.back()];
            float run = (last.at - first.at) / (chain.size() - 1);
            float start = first.at, end = last.at + run;
            int side = 2 - first.axis;
            Point center{};
            center[first.axis] = (start + end) / 2;
            center[side] = (first.lo + first.hi) / 2;
            center[1] = first.bottom;
            Point dim{ first.hi - first.lo, 0, std::abs(end - start) };
            float yaw = first.axis == 0 ? (run > 0 ? 1.5707963268f : -1.5707963268f) : (run > 0 ? 0 : 3.1415926536f);
            Plane(r, center, yaw, dim, last.top - first.bottom, Surface::Stair);
            ++r.stairs;
            for (auto id : chain) {
                consumed[id] = true;
                for (auto t : risers[id].ids)
                    remove[t] = true;
            }
            // Remove only horizontal tread triangles fully inside the detected flight.
            for (size_t t = 0; t < originalCount; ++t) {
                if (r.triangles[t].surface != Surface::Flat)
                    continue;
                bool inside = true;
                for (auto p : { r.triangles[t].a, r.triangles[t].b, r.triangles[t].c })
                    if (p[first.axis] < std::min(start, end) - .001f || p[first.axis] > std::max(start, end) + .001f ||
                        p[side] < first.lo - .001f || p[side] > first.hi + .001f || p[1] <= first.bottom + .001f ||
                        p[1] > last.top + .001f)
                        inside = false;
                if (inside)
                    remove[t] = true;
            }
        } else if (step <= options.microTolerance) {
            auto a = risers[i];
            int side = 2 - a.axis;
            auto n = Normal(r.triangles[a.ids[0]]);
            float lowerDirection = n[a.axis] > 0 ? 1.f : -1.f;
            float run = std::max(.10f, step * 4);
            Point lower{}, upper{};
            lower[a.axis] = a.at + lowerDirection * run;
            upper[a.axis] = a.at;
            lower[side] = upper[side] = (a.lo + a.hi) / 2;
            lower[1] = a.bottom;
            upper[1] = a.top;
            float floor;
            Point normal;
            // Require lower supporting floor; do not bridge unsupported gaps.
            if (Floor(r.triangles, lower, floor, normal) && std::abs(floor - a.bottom) < .01f) {
                Point center = lower;
                center[a.axis] = (lower[a.axis] + upper[a.axis]) / 2;
                Point dim{ a.hi - a.lo, 0, run };
                float yaw = a.axis == 0 ? (lowerDirection > 0 ? -1.5707963268f : 1.5707963268f)
                                        : (lowerDirection > 0 ? 3.1415926536f : 0);
                Plane(r, center, yaw, dim, step, Surface::Curb);
                ++r.edges;
                for (auto id : a.ids)
                    remove[id] = true;
                consumed[i] = true;
            }
        }
    }
    // Shared-edge topology identifies high ledge tops without coordinate tags.
    std::map<std::pair<Point, Point>, std::vector<size_t>> shared;
    for (size_t i = 0; i < originalCount; ++i)
        if (!remove[i]) {
            auto& t = r.triangles[i];
            for (auto edge : { std::make_pair(t.a, t.b), std::make_pair(t.b, t.c), std::make_pair(t.c, t.a) }) {
                if (edge.second < edge.first)
                    std::swap(edge.first, edge.second);
                shared[edge].push_back(i);
            }
        }
    for (auto& entry : shared) {
        for (auto i : entry.second) {
            auto& t = r.triangles[i];
            if (t.surface != Surface::Wall)
                continue;
            float height = std::max({ t.a[1], t.b[1], t.c[1] }) - std::min({ t.a[1], t.b[1], t.c[1] });
            if (height > .35f)
                for (auto j : entry.second)
                    if (r.triangles[j].surface == Surface::Flat)
                        r.triangles[j].surface = Surface::Ledge;
        }
    }
    std::vector<Triangle> kept;
    for (size_t i = 0; i < r.triangles.size(); ++i)
        if (i >= originalCount || !remove[i])
            kept.push_back(r.triangles[i]);
    r.triangles = std::move(kept);
    return r;
}
std::vector<float> Flatten(const Result& r) {
    std::vector<float> out;
    out.reserve(r.triangles.size() * 9);
    for (auto& t : r.triangles)
        for (auto p : { t.a, t.b, t.c })
            out.insert(out.end(), p.begin(), p.end());
    return out;
}
} // namespace NativeSkateGeometry

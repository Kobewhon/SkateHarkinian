#pragma once
#include <array>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <chrono>
namespace NativeSkateGrindCompiler {
using Point = std::array<float, 3>;
enum class Category { Ledge, Rail, Curb, Coping, StairEdge, RoofEdge, Reject };
struct Edge {
    uint64_t id = 0;
    Point a{}, b{}, normal{};
    Category category = Category::Reject;
    bool seam = false;
};
struct Spline {
    uint64_t id = 0;
    Category category = Category::Ledge;
    std::vector<Point> points;
};
struct Result {
    std::vector<Edge> edges;
    std::vector<Spline> splines;
    uint32_t triangles = 0, seams = 0, candidates = 0;
    float length = 0;
    double extractionMs = 0, classificationMs = 0, weldMs = 0;
    std::string error;
};
struct Override {
    bool reject = false, accept = false, split = false;
    int category = -1;
    uint32_t joinGroup = 0;
};
using Overrides = std::map<uint64_t, Override>;
inline Point Sub(Point a, Point b) {
    return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}
inline float Dot(Point a, Point b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline float Length(Point a) {
    return std::sqrt(Dot(a, a));
}
inline Point Normal(Point a) {
    float l = Length(a);
    return l > 1e-7f ? Point{ a[0] / l, a[1] / l, a[2] / l } : Point{};
}
inline Point Cross(Point a, Point b) {
    return { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
}
using Key = std::array<int64_t, 3>;
inline Key KeyOf(Point a) {
    return { (int64_t)std::llround(a[0] * 1000), (int64_t)std::llround(a[1] * 1000),
             (int64_t)std::llround(a[2] * 1000) };
}
inline uint64_t Id(Key a, Key b) {
    if (b < a)
        std::swap(a, b);
    uint64_t h = 1469598103934665603ull;
    for (auto k : { a, b })
        for (int64_t n : k)
            for (unsigned j = 0; j < 8; ++j) {
                h ^= ((uint64_t)n >> (j * 8)) & 255;
                h *= 1099511628211ull;
            }
    return h;
}
inline const char* Name(Category c) {
    const char* names[] = { "LEDGE", "RAIL", "CURB", "COPING", "STAIR_EDGE", "ROOF_EDGE", "REJECT" };
    return names[(unsigned)c];
}
inline Result Compile(const std::vector<float>& triangles, const Overrides& overrides = {}) {
    using Clock = std::chrono::steady_clock;
    auto begin = Clock::now();
    Result out;
    if (triangles.size() % 9) {
        out.error = "Invalid triangle packet";
        return out;
    }
    out.triangles = (uint32_t)(triangles.size() / 9);
    struct Face {
        Point p[3], n;
    };
    std::vector<Face> faces;
    std::map<std::pair<Key, Key>, std::vector<std::pair<unsigned, unsigned>>> adjacency;
    for (size_t i = 0; i < triangles.size(); i += 9) {
        Face f{};
        bool valid = true;
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned k = 0; k < 3; ++k) {
                f.p[j][k] = triangles[i + j * 3 + k];
                valid &= std::isfinite(f.p[j][k]) && std::abs(f.p[j][k]) < 100000;
            }
        if (!valid)
            continue;
        f.n = Normal(Cross(Sub(f.p[1], f.p[0]), Sub(f.p[2], f.p[0])));
        if (Length(f.n) < .9f)
            continue;
        unsigned index = (unsigned)faces.size();
        faces.push_back(f);
        for (unsigned j = 0; j < 3; ++j) {
            Key a = KeyOf(f.p[j]), b = KeyOf(f.p[(j + 1) % 3]);
            if (b < a)
                std::swap(a, b);
            adjacency[{ a, b }].push_back({ index, j });
        }
    }
    std::map<std::pair<int, int>, std::vector<unsigned>> cells;
    std::vector<unsigned> wide;
    for (unsigned i = 0; i < faces.size(); ++i) {
        const auto& f = faces[i];
        int x0 = (int)std::floor(std::min({ f.p[0][0], f.p[1][0], f.p[2][0] }) / 2),
            x1 = (int)std::floor(std::max({ f.p[0][0], f.p[1][0], f.p[2][0] }) / 2),
            z0 = (int)std::floor(std::min({ f.p[0][2], f.p[1][2], f.p[2][2] }) / 2),
            z1 = (int)std::floor(std::max({ f.p[0][2], f.p[1][2], f.p[2][2] }) / 2);
        if ((int64_t)(x1 - x0 + 1) * (z1 - z0 + 1) > 4096) {
            wide.push_back(i);
            continue;
        }
        for (int x = x0; x <= x1; ++x)
            for (int z = z0; z <= z1; ++z)
                cells[{ x, z }].push_back(i);
    }
    auto clear = [&](Point p) {
        auto refs = cells[std::make_pair((int)std::floor(p[0] / 2), (int)std::floor(p[2] / 2))];
        refs.insert(refs.end(), wide.begin(), wide.end());
        for (unsigned i : refs) {
            const auto& f = faces[i];
            float den =
                (f.p[1][2] - f.p[2][2]) * (f.p[0][0] - f.p[2][0]) + (f.p[2][0] - f.p[1][0]) * (f.p[0][2] - f.p[2][2]);
            if (std::abs(den) < 1e-8f)
                continue;
            float u = ((f.p[1][2] - f.p[2][2]) * (p[0] - f.p[2][0]) + (f.p[2][0] - f.p[1][0]) * (p[2] - f.p[2][2])) /
                      den,
                  v = ((f.p[2][2] - f.p[0][2]) * (p[0] - f.p[2][0]) + (f.p[0][0] - f.p[2][0]) * (p[2] - f.p[2][2])) /
                      den;
            float y = u * f.p[0][1] + v * f.p[1][1] + (1 - u - v) * f.p[2][1];
            if (u >= -.001f && v >= -.001f && u + v <= 1.001f && y > p[1] + .04f && y < p[1] + .9f)
                return false;
        }
        return true;
    };
    out.extractionMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    begin = Clock::now();
    for (const auto& entry : adjacency) {
        auto refs = entry.second;
        const auto& f = faces[refs[0].first];
        Edge e;
        e.id = Id(entry.first.first, entry.first.second);
        e.a = f.p[refs[0].second];
        e.b = f.p[(refs[0].second + 1) % 3];
        float len = Length(Sub(e.b, e.a));
        if (refs.size() == 2) {
            const Face* top = &f;
            const Face* side = &faces[refs[1].first];
            unsigned topEdge = refs[0].second;
            if (side->n[1] > top->n[1]) {
                std::swap(top, side);
                topEdge = refs[1].second;
            }
            float cosine = Dot(top->n, side->n);
            if (cosine > .98f) {
                e.seam = true;
                ++out.seams;
            } else if (len >= .3f && std::abs(e.b[1] - e.a[1]) / len <= .5f && top->n[1] >= .65f &&
                       std::abs(side->n[1]) < .55f && cosine < .7f &&
                       Dot(side->n, Sub(top->p[(topEdge + 2) % 3], e.a)) < -.001f) {
                float bottom = std::min({ side->p[0][1], side->p[1][1], side->p[2][1] });
                float height = std::min(e.a[1], e.b[1]) - bottom;
                if (height >= .04f) {
                    e.normal = top->n;
                    e.category = height < .25f      ? Category::Curb
                                 : top->n[1] < .85f ? Category::RoofEdge
                                                    : Category::Ledge;
                    ++out.candidates;
                }
            }
        }
        auto ov = overrides.find(e.id);
        if (ov != overrides.end()) {
            if (ov->second.reject)
                e.category = Category::Reject;
            else if (ov->second.accept && !e.seam && len >= .3f) {
                e.category = Category::Ledge;
                e.normal = { 0, 1, 0 };
            }
            if (e.category != Category::Reject && ov->second.category >= 0 && ov->second.category < 6)
                e.category = (Category)ov->second.category;
        }
        if (e.category != Category::Reject &&
            !clear({ (e.a[0] + e.b[0]) * .5f, (e.a[1] + e.b[1]) * .5f, (e.a[2] + e.b[2]) * .5f }))
            e.category = Category::Reject;
        out.edges.push_back(e);
    }
    // A thin elevated top has parallel ledge edges. Use one centerline instead of
    // two competing rails. Coplanar diagonals never enter this classifier.
    std::vector<bool> consumed(out.edges.size());
    for (size_t i = 0; i < out.edges.size(); ++i) {
        auto& a = out.edges[i];
        if (a.category != Category::Ledge || consumed[i] || Length(Sub(a.b, a.a)) < 1)
            continue;
        for (size_t j = i + 1; j < out.edges.size(); ++j) {
            auto& b = out.edges[j];
            if (b.category != Category::Ledge || consumed[j])
                continue;
            Point ba = b.a, bb = b.b;
            if (Dot(Sub(a.b, a.a), Sub(bb, ba)) < 0)
                std::swap(ba, bb);
            float width = Length(Sub(a.a, ba));
            if (width < .03f || width > .25f || std::abs(a.a[1] - ba[1]) > .03f ||
                std::abs(Length(Sub(a.b, bb)) - width) > .02f ||
                Dot(Normal(Sub(a.b, a.a)), Normal(Sub(bb, ba))) < .995f)
                continue;
            for (unsigned k = 0; k < 3; ++k) {
                a.a[k] = (a.a[k] + ba[k]) * .5f;
                a.b[k] = (a.b[k] + bb[k]) * .5f;
            }
            a.category = Category::Rail;
            b.category = Category::Reject;
            consumed[j] = true;
            break;
        }
    }
    out.classificationMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    begin = Clock::now();
    std::map<Key, std::vector<size_t>> endpoints;
    for (size_t i = 0; i < out.edges.size(); ++i)
        if (out.edges[i].category != Category::Reject) {
            endpoints[KeyOf(out.edges[i].a)].push_back(i);
            endpoints[KeyOf(out.edges[i].b)].push_back(i);
        }
    std::vector<bool> used(out.edges.size());
    for (size_t i = 0; i < out.edges.size(); ++i) {
        const auto& e = out.edges[i];
        if (e.category == Category::Reject || used[i])
            continue;
        used[i] = true;
        Spline line{ e.id, e.category, { e.a, e.b } };
        auto initialOverride = overrides.find(e.id);
        bool isolated = initialOverride != overrides.end() && initialOverride->second.split;
        for (int end = 0; !isolated && end < 2; ++end) {
            while (true) {
                Point last = end ? line.points.front() : line.points.back();
                Point prev = end ? line.points[1] : line.points[line.points.size() - 2];
                Point direction = Normal(Sub(last, prev));
                bool joined = false;
                for (size_t next : endpoints[KeyOf(last)]) {
                    const auto& n = out.edges[next];
                    auto ov = overrides.find(n.id);
                    if (used[next] || n.category != line.category || (ov != overrides.end() && ov->second.split))
                        continue;
                    Point other = KeyOf(n.a) == KeyOf(last) ? n.b : n.a;
                    bool requestedJoin = initialOverride != overrides.end() && ov != overrides.end() &&
                                         initialOverride->second.joinGroup != 0 &&
                                         initialOverride->second.joinGroup == ov->second.joinGroup;
                    if (Dot(direction, Normal(Sub(other, last))) < (requestedJoin ? .8f : .94f) ||
                        Dot(e.normal, n.normal) < .94f)
                        continue;
                    used[next] = true;
                    if (end)
                        line.points.insert(line.points.begin(), other);
                    else
                        line.points.push_back(other);
                    joined = true;
                    break;
                }
                if (!joined)
                    break;
            }
        }
        float len = 0;
        for (size_t j = 1; j < line.points.size(); ++j)
            len += Length(Sub(line.points[j], line.points[j - 1]));
        if (len >= .5f) {
            out.length += len;
            out.splines.push_back(std::move(line));
        }
    }
    out.weldMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
    return out;
}
void InvalidateOverrides();
Overrides LoadOverrides(const std::string& path, int scene, std::string& error);
} // namespace NativeSkateGrindCompiler

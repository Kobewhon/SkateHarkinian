#pragma once
#include <array>
#include <vector>
#include <cstdint>
namespace NativeSkateGeometry {
using Point = std::array<float, 3>;
enum class Surface { Flat, Bank, Stair, Curb, Ledge, Wall, TooSteep, Micro };
struct Triangle {
    Point a, b, c;
    Surface surface = Surface::Flat;
    bool generated = false, visible = false;
};
struct Options {
    bool stairs = true;
    float microTolerance = 0.025f;
};
struct Result {
    std::vector<Triangle> triangles;
    uint32_t stairs = 0, edges = 0, rejected = 0, ramps = 0, banks = 0;
};
Surface Classify(const Triangle& t);
bool Floor(const std::vector<Triangle>& triangles, const Point& position, float& height, Point& normal);
Result Adapt(const std::vector<float>& original, const Options& options, bool enabled);
void Plane(Result& result, Point center, float yaw, Point dimensions, float rise, Surface type);
std::vector<float> Flatten(const Result& result);
} // namespace NativeSkateGeometry

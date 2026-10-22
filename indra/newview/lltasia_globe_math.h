#pragma once
#include <algorithm>
#include <cmath>
#include <array>

namespace TasiaGlobe
{
constexpr double pi = 3.14159265358979323846;
struct Point { double x, y, z; };
struct Vertex { Point p; double u, v; };
struct Polygon { std::array<Vertex,4> points; int count = 0; };
inline Polygon frontTriangle(Vertex a, Vertex b, Vertex c)
{
    Polygon result;
    const std::array<Vertex,3> input{{a,b,c}};
    auto previous = input.back();
    for (const auto& current : input)
    {
        if ((previous.p.z >= 0) != (current.p.z >= 0))
        {
            const double t = previous.p.z / (previous.p.z - current.p.z);
            result.points[result.count++] = {{previous.p.x + t*(current.p.x-previous.p.x),
                previous.p.y + t*(current.p.y-previous.p.y), 0},
                previous.u + t*(current.u-previous.u), previous.v + t*(current.v-previous.v)};
        }
        if (current.p.z >= 0) result.points[result.count++] = current;
        previous = current;
    }
    return result;
}
inline Point project(double longitude, double latitude, double yaw, double pitch)
{
    const double x = std::cos(latitude) * std::sin(longitude + yaw);
    const double z = std::cos(latitude) * std::cos(longitude + yaw);
    const double y = std::sin(latitude);
    return {x, y * std::cos(pitch) - z * std::sin(pitch),
        y * std::sin(pitch) + z * std::cos(pitch)};
}
inline double zoom(double current, int clicks)
{
    return std::clamp(current * std::pow(0.9, std::clamp(clicks, -20, 20)), 0.65, 1.8);
}
inline double atlas(double value, double low, double high, double radians)
{
    return high <= low ? 0.0 : (2.0 * std::clamp((value - low) / (high - low), 0.0, 1.0) - 1.0) * radians;
}
}

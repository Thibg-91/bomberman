#include "mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

float cross(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool insideTriangle(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c)
{
    const float d1 = cross(a, b, p);
    const float d2 = cross(b, c, p);
    const float d3 = cross(c, a, p);
    const bool hasNegative = d1 < 0.f || d2 < 0.f || d3 < 0.f;
    const bool hasPositive = d1 > 0.f || d2 > 0.f || d3 > 0.f;
    return !(hasNegative && hasPositive);
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
Mesh::Mesh(sf::VertexArray& vertices) noexcept
    : m_vertices(vertices)
{
}

// =========================
// Static Methods
// =========================
int Mesh::segmentsFor(float radius) noexcept
{
    return std::clamp(static_cast<int>(radius * 0.8f) + 12, 12, 120);
}

// =========================
// Public Methods
// =========================
void Mesh::triangle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color ca, sf::Color cb, sf::Color cc)
{
    m_vertices.append(sf::Vertex{a, ca});
    m_vertices.append(sf::Vertex{b, cb});
    m_vertices.append(sf::Vertex{c, cc});
}

void Mesh::triangle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color color)
{
    triangle(a, b, c, color, color, color);
}

void Mesh::quad(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3, sf::Color color)
{
    triangle(p0, p1, p2, color);
    triangle(p0, p2, p3, color);
}

void Mesh::rect(float x, float y, float w, float h, sf::Color color)
{
    quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, color);
}

void Mesh::polygon(const std::vector<sf::Vector2f>& points, sf::Color color)
{
    for (std::size_t i = 1; i + 1 < points.size(); ++i)
        triangle(points[0], points[i], points[i + 1], color);
}

void Mesh::fan(sf::Vector2f center, const std::vector<sf::Vector2f>& points, sf::Color color)
{
    for (std::size_t i = 0; i < points.size(); ++i)
        triangle(center, points[i], points[(i + 1) % points.size()], color);
}

void Mesh::fillPolygon(const std::vector<sf::Vector2f>& input, sf::Color color)
{
    // Ear clipping. Consecutive duplicates (e.g. where two curves meet) would
    // create degenerate ears, so drop them first.
    std::vector<sf::Vector2f> points;
    for (const sf::Vector2f p : input)
        if (points.empty() || std::hypot(p.x - points.back().x, p.y - points.back().y) > 1e-4f)
            points.push_back(p);
    while (points.size() > 1 && std::hypot(points.front().x - points.back().x, points.front().y - points.back().y) <= 1e-4f)
        points.pop_back();
    if (points.size() < 3)
        return;

    float area = 0.f;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const sf::Vector2f a = points[i];
        const sf::Vector2f b = points[(i + 1) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }
    if (area < 0.f)
        std::reverse(points.begin(), points.end());

    std::vector<std::size_t> remaining(points.size());
    std::iota(remaining.begin(), remaining.end(), 0u);
    while (remaining.size() > 3)
    {
        bool clipped = false;
        for (std::size_t i = 0; i < remaining.size(); ++i)
        {
            const std::size_t prev = remaining[(i + remaining.size() - 1) % remaining.size()];
            const std::size_t current = remaining[i];
            const std::size_t next = remaining[(i + 1) % remaining.size()];
            const sf::Vector2f a = points[prev];
            const sf::Vector2f b = points[current];
            const sf::Vector2f c = points[next];
            if (cross(a, b, c) <= 1e-6f)
                continue; // reflex or flat corner: not an ear

            const bool containsOther = std::any_of(remaining.begin(), remaining.end(), [&](std::size_t k) {
                return k != prev && k != current && k != next && insideTriangle(points[k], a, b, c);
            });
            if (containsOther)
                continue;

            triangle(a, b, c, color);
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped)
            break; // not a simple polygon: draw what is left as a fan
    }
    for (std::size_t i = 1; i + 1 < remaining.size(); ++i)
        triangle(points[remaining[0]], points[remaining[i]], points[remaining[i + 1]], color);
}

void Mesh::ellipse(sf::Vector2f center, float rx, float ry, sf::Color color, float rotation, int segments)
{
    if (rx <= 0.f || ry <= 0.f)
        return;
    fan(center, ellipsePoints(center, rx, ry, rotation, segments > 0 ? segments : segmentsFor(std::max(rx, ry))),
        color);
}

void Mesh::circle(sf::Vector2f center, float r, sf::Color color, int segments)
{
    ellipse(center, r, r, color, 0.f, segments);
}

void Mesh::ring(sf::Vector2f center, float r, float width, sf::Color color, int segments)
{
    const float inner = std::max(0.f, r - width / 2.f);
    const float outer = r + width / 2.f;
    const int count = segments > 0 ? segments : segmentsFor(outer);
    for (int i = 0; i < count; ++i)
    {
        const float a0 = 2.f * Pi * static_cast<float>(i) / static_cast<float>(count);
        const float a1 = 2.f * Pi * static_cast<float>(i + 1) / static_cast<float>(count);
        const sf::Vector2f d0{std::cos(a0), std::sin(a0)};
        const sf::Vector2f d1{std::cos(a1), std::sin(a1)};
        quad(center + d0 * outer, center + d1 * outer, center + d1 * inner, center + d0 * inner, color);
    }
}

void Mesh::glowSector(sf::Vector2f center, float r, sf::Color color, float from, float to, int segments)
{
    sf::Color rim = color;
    rim.a = 0;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = from + (to - from) * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = from + (to - from) * static_cast<float>(i + 1) / static_cast<float>(segments);
        triangle(center, center + sf::Vector2f{std::cos(a0), std::sin(a0)} * r,
                 center + sf::Vector2f{std::cos(a1), std::sin(a1)} * r, color, rim, rim);
    }
}

void Mesh::line(sf::Vector2f a, sf::Vector2f b, float width, sf::Color color, bool roundCaps)
{
    const sf::Vector2f d = b - a;
    const float length = std::hypot(d.x, d.y);
    if (length > 0.f)
    {
        const sf::Vector2f n{-d.y / length * width / 2.f, d.x / length * width / 2.f};
        quad(a + n, b + n, b - n, a - n, color);
    }
    if (roundCaps)
    {
        circle(a, width / 2.f, color);
        circle(b, width / 2.f, color);
    }
}

void Mesh::polyline(const std::vector<sf::Vector2f>& points, float width, sf::Color color, bool roundJoins)
{
    for (std::size_t i = 0; i + 1 < points.size(); ++i)
        line(points[i], points[i + 1], width, color);
    if (roundJoins)
        for (const sf::Vector2f p : points)
            circle(p, width / 2.f, color);
}

void Mesh::stroke(const std::vector<sf::Vector2f>& points, float width, sf::Color color)
{
    if (points.size() < 2)
        return;
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        line(points[i], points[(i + 1) % points.size()], width, color);
        circle(points[i], width / 2.f, color);
    }
}

void Mesh::arc(sf::Vector2f center, float r, float from, float to, float width, sf::Color color, int segments)
{
    std::vector<sf::Vector2f> points;
    for (int i = 0; i <= segments; ++i)
    {
        const float a = from + (to - from) * static_cast<float>(i) / static_cast<float>(segments);
        points.push_back(center + sf::Vector2f{std::cos(a), std::sin(a)} * r);
    }
    polyline(points, width, color);
}

// =========================
// Free functions
// =========================
std::vector<sf::Vector2f> ellipsePoints(sf::Vector2f center, float rx, float ry, float rotation, int segments)
{
    std::vector<sf::Vector2f> points;
    points.reserve(static_cast<std::size_t>(std::max(segments, 0)));
    const float cosR = std::cos(rotation);
    const float sinR = std::sin(rotation);
    for (int i = 0; i < segments; ++i)
    {
        const float a = 2.f * Pi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(a) * rx;
        const float y = std::sin(a) * ry;
        points.push_back({center.x + x * cosR - y * sinR, center.y + x * sinR + y * cosR});
    }
    return points;
}

std::vector<sf::Vector2f> roundedRectPoints(float x, float y, float w, float h, float r, int perCorner)
{
    r = std::clamp(r, 0.f, std::min(w, h) / 2.f);
    const std::array<sf::Vector2f, 4> centers = {sf::Vector2f{x + w - r, y + r}, sf::Vector2f{x + w - r, y + h - r},
                                                 sf::Vector2f{x + r, y + h - r}, sf::Vector2f{x + r, y + r}};
    std::vector<sf::Vector2f> points;
    for (std::size_t corner = 0; corner < centers.size(); ++corner)
    {
        const float start = -Pi / 2.f + static_cast<float>(corner) * Pi / 2.f;
        for (int i = 0; i <= perCorner; ++i)
        {
            const float a = start + (Pi / 2.f) * static_cast<float>(i) / static_cast<float>(perCorner);
            points.push_back({centers[corner].x + std::cos(a) * r, centers[corner].y + std::sin(a) * r});
        }
    }
    return points;
}

std::vector<sf::Vector2f> quadraticBezier(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, int segments)
{
    std::vector<sf::Vector2f> points;
    for (int i = 0; i <= segments; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float u = 1.f - t;
        points.push_back(p0 * (u * u) + p1 * (2.f * u * t) + p2 * (t * t));
    }
    return points;
}

} // namespace Bomberman

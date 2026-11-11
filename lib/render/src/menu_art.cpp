#include "menu_art.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Bomberman
{

namespace MenuArt
{

namespace
{

std::vector<sf::Vector2f> rectPoints(float x, float y, float w, float h, float r)
{
    return r > 0.f ? roundedRectPoints(x, y, w, h, r, 6)
                   : std::vector<sf::Vector2f>{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
}

// A filled rounded rectangle with an ink outline, turned around pivot.
void outlinedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Color fill, float outline,
                  sf::Vector2f pivot = {}, float angle = 0.f, sf::Color ink = Ink)
{
    const std::vector<sf::Vector2f> points = turned(rectPoints(x, y, w, h, r), pivot, angle);
    mesh.polygon(points, fill);
    mesh.stroke(points, outline, ink);
}

std::vector<sf::Vector2f> curve(sf::Vector2f from, sf::Vector2f control, sf::Vector2f to, int segments = 12)
{
    return quadraticBezier(from, control, to, segments);
}

} // namespace

sf::Color dimmed(sf::Color color, float brightness)
{
    auto scale = [brightness](std::uint8_t c) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(c) * brightness, 0.f, 255.f));
    };
    return sf::Color(scale(color.r), scale(color.g), scale(color.b), color.a);
}

std::vector<sf::Vector2f> turned(std::vector<sf::Vector2f> points, sf::Vector2f pivot, float angle)
{
    if (angle == 0.f)
        return points;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    for (sf::Vector2f& p : points)
    {
        const sf::Vector2f d = p - pivot;
        p = pivot + sf::Vector2f(d.x * c - d.y * s, d.x * s + d.y * c);
    }
    return points;
}

void roundedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Color fill)
{
    mesh.polygon(roundedRectPoints(x, y, w, h, r, 6), fill);
}

void roundedFrame(Mesh& mesh, float x, float y, float w, float h, float r, float width, sf::Color color)
{
    mesh.stroke(roundedRectPoints(x, y, w, h, r, 6), width, color);
}

void dashedStroke(Mesh& mesh, const std::vector<sf::Vector2f>& closed, float width, float dash, float gap,
                  sf::Color color)
{
    if (closed.size() < 2 || dash <= 0.f)
        return;
    std::vector<sf::Vector2f> path = closed;
    path.push_back(closed.front());

    // Walk the outline, drawing while inside a dash.
    float position = 0.f; // within the current dash + gap period
    std::vector<sf::Vector2f> current = {path.front()};
    for (std::size_t i = 0; i + 1 < path.size(); ++i)
    {
        sf::Vector2f a = path[i];
        const sf::Vector2f b = path[i + 1];
        float remaining = std::hypot(b.x - a.x, b.y - a.y);
        while (remaining > 0.f)
        {
            const bool drawing = position < dash;
            const float boundary = drawing ? dash : dash + gap;
            const float advance = std::min(remaining, boundary - position);
            const float t = advance / std::hypot(b.x - a.x, b.y - a.y);
            const sf::Vector2f next = a + (b - a) * t;
            if (drawing)
                current.push_back(next);
            position += advance;
            remaining -= advance;
            a = next;
            if (position >= dash && drawing)
            {
                if (current.size() >= 2)
                    mesh.polyline(current, width, color, true);
                current.clear();
            }
            if (position >= dash + gap)
            {
                position = 0.f;
                current = {a};
            }
        }
    }
    if (current.size() >= 2)
        mesh.polyline(current, width, color, true);
}

void deckFloor(Mesh& mesh, sf::Vector2f size)
{
    mesh.rect(0.f, 0.f, size.x, size.y, Deck);
    for (float y = 60.f; y < size.y; y += 60.f)
        mesh.rect(0.f, y - 2.f, size.x, 3.f, Seam);
}

void launchButton(Mesh& mesh, sf::Vector2f c, float r, bool enabled, float elapsed, float frequency)
{
    const float scale = enabled ? 1.f + std::sin(elapsed * Pi * 2.f * frequency) * 0.05f : 1.f;
    const float radius = r * scale;
    if (enabled)
        mesh.circle(c, radius * 1.3f, GoldGlow);
    mesh.circle(c, radius, enabled ? Gold : LaunchOff);
    mesh.ring(c, radius, radius * 0.1f, Ink);
    const std::vector<sf::Vector2f> play = {
        {c.x - radius * 0.3f, c.y - radius * 0.45f}, {c.x - radius * 0.3f, c.y + radius * 0.45f}, {c.x + radius * 0.5f, c.y}};
    mesh.polygon(play, Ink);
    mesh.stroke(play, radius * 0.08f, Ink);
}

void arrow(Mesh& mesh, sf::Vector2f c, float direction, float size)
{
    const std::vector<sf::Vector2f> points = {{c.x + direction * size, c.y},
                                              {c.x - direction * size * 0.6f, c.y - size},
                                              {c.x - direction * size * 0.6f, c.y + size}};
    mesh.polygon(points, Gold);
    mesh.stroke(points, size * 0.18f, Ink);
}

void crown(Mesh& mesh, sf::Vector2f c, float s, float brightness)
{
    const std::vector<sf::Vector2f> points = {
        {c.x - s, c.y + s * 0.5f}, {c.x - s, c.y - s * 0.45f}, {c.x - s * 0.5f, c.y},     {c.x, c.y - s * 0.7f},
        {c.x + s * 0.5f, c.y},     {c.x + s, c.y - s * 0.45f}, {c.x + s, c.y + s * 0.5f}};
    const sf::Color ink = dimmed(Ink, brightness);
    mesh.fillPolygon(points, dimmed(Gold, brightness));
    mesh.stroke(points, s * 0.14f, ink);
    mesh.circle({c.x, c.y + s * 0.12f}, s * 0.16f, dimmed(SeatColors[0], brightness));
    mesh.ring({c.x, c.y + s * 0.12f}, s * 0.16f, s * 0.06f, ink);
}

void hourglass(Mesh& mesh, sf::Vector2f c, float s, float elapsed)
{
    const float phase = std::fmod(std::max(elapsed, 0.f), 2.f);
    const float turn = phase > 1.7f ? (phase - 1.7f) / 0.3f * Pi : 0.f;
    const float sand = std::min(1.f, phase / 1.7f);

    auto shape = [&](std::vector<sf::Vector2f> points, sf::Color fill, float outline, bool convex) {
        points = turned(std::move(points), c, turn);
        if (convex)
            mesh.polygon(points, fill);
        else
            mesh.fillPolygon(points, fill);
        if (outline > 0.f)
            mesh.stroke(points, outline, Ink);
    };
    shape(rectPoints(c.x - s * 0.6f, c.y - s - s * 0.12f, s * 1.2f, s * 0.18f, s * 0.06f), Wood, s * 0.06f, true);
    shape(rectPoints(c.x - s * 0.6f, c.y + s - s * 0.06f, s * 1.2f, s * 0.18f, s * 0.06f), Wood, s * 0.06f, true);
    shape({{c.x - s * 0.45f, c.y - s},
           {c.x + s * 0.45f, c.y - s},
           {c.x + s * 0.06f, c.y},
           {c.x + s * 0.45f, c.y + s},
           {c.x - s * 0.45f, c.y + s},
           {c.x - s * 0.06f, c.y}},
          sf::Color(220, 235, 245, 140), s * 0.07f, false);
    const float top = 1.f - sand;
    if (top > 0.f)
        shape({{c.x - s * 0.45f * top, c.y - s * top}, {c.x + s * 0.45f * top, c.y - s * top}, {c.x, c.y - s * 0.05f}},
              Gold, 0.f, true);
    if (sand > 0.f)
        shape({{c.x - s * 0.4f * sand, c.y + s}, {c.x + s * 0.4f * sand, c.y + s}, {c.x, c.y + s * (1.f - 0.55f * sand)}},
              Gold, 0.f, true);
}

void waves(Mesh& mesh, sf::Vector2f c, float s, float elapsed, sf::Color lit, sf::Color unlit, bool animate)
{
    mesh.circle(c, s * 0.12f, lit);
    mesh.ring(c, s * 0.12f, s * 0.04f, Ink);
    const int step = static_cast<int>(std::floor(std::max(elapsed, 0.f) * 2.5f)) % 4;
    for (int k = 1; k <= 3; ++k)
    {
        const float r = s * 0.3f * static_cast<float>(k);
        mesh.polyline(curve({c.x - r * 0.8f, c.y - r * 0.5f}, {c.x, c.y - r * 1.25f}, {c.x + r * 0.8f, c.y - r * 0.5f}),
                      s * 0.1f, !animate || step >= k ? lit : unlit, true);
    }
}

void ship(Mesh& mesh, sf::Vector2f c, float s, float brightness)
{
    const float k = s / 100.f;
    auto at = [&](float x, float y) { return sf::Vector2f(c.x + (x - 50.f) * k, c.y + (y - 50.f) * k); };
    const sf::Color ink = dimmed(Ink, brightness);

    const std::vector<sf::Vector2f> mast = rectPoints(at(48, 8).x, at(48, 8).y, 4.f * k, 56.f * k, 0.f);
    mesh.polygon(mast, dimmed(DarkWood, brightness));
    mesh.stroke(mast, 1.5f * k, ink);
    mesh.triangle(at(52, 8), at(66, 12), at(52, 16), ink);

    std::vector<sf::Vector2f> sail = curve(at(26, 18), at(50, 24), at(74, 18));
    const std::vector<sf::Vector2f> bottom = curve(at(76, 52), at(50, 58), at(24, 52));
    sail.insert(sail.end(), bottom.begin(), bottom.end());
    mesh.fillPolygon(sail, dimmed(Bone, brightness));
    mesh.stroke(sail, 2.5f * k, ink);

    const std::vector<sf::Vector2f> hull = {at(6, 60), at(94, 60), at(82, 86), at(18, 86)};
    mesh.polygon(hull, dimmed(Wood, brightness));
    mesh.stroke(hull, 3.f * k, ink);
    mesh.rect(at(10, 64).x, at(10, 64).y, 80.f * k, 4.f * k, dimmed(Gold, brightness));
    for (const float x : {30.f, 50.f, 70.f})
    {
        mesh.circle(at(x, 75), 3.5f * k, dimmed(BadgeFill, brightness));
        mesh.ring(at(x, 75), 3.5f * k, 1.2f * k, dimmed(Gold, brightness));
    }
}

void spyglass(Mesh& mesh, sf::Vector2f c, float s, float degrees, float brightness)
{
    const float angle = degrees * Pi / 180.f;
    const sf::Color ink = dimmed(Ink, brightness);
    outlinedRect(mesh, c.x - s * 0.9f, c.y - s * 0.14f, s * 0.7f, s * 0.28f, s * 0.06f, dimmed(Wood, brightness),
                 s * 0.06f, c, angle, ink);
    outlinedRect(mesh, c.x - s * 0.25f, c.y - s * 0.19f, s * 0.6f, s * 0.38f, s * 0.06f, dimmed(Gold, brightness),
                 s * 0.06f, c, angle, ink);
    outlinedRect(mesh, c.x + s * 0.3f, c.y - s * 0.26f, s * 0.55f, s * 0.52f, s * 0.08f,
                 dimmed(sf::Color(138, 90, 46), brightness), s * 0.06f, c, angle, ink);
    const std::vector<sf::Vector2f> lens = turned(
        ellipsePoints({c.x + s * 0.86f, c.y}, s * 0.08f, s * 0.24f, 0.f, Mesh::segmentsFor(s * 0.24f)), c, angle);
    mesh.polygon(lens, dimmed(sf::Color(191, 227, 242), brightness));
    mesh.stroke(lens, s * 0.05f, ink);
}

void waitingDots(Mesh& mesh, sf::Vector2f c, float elapsed, float radius, float gap)
{
    for (int k = 0; k < 3; ++k)
    {
        const float alpha = 0.25f + 0.75f * std::max(0.f, std::sin(elapsed * 4.f - static_cast<float>(k) * 0.8f));
        const sf::Vector2f dot(c.x + static_cast<float>(k - 1) * gap, c.y);
        mesh.circle(dot, radius, sf::Color(Bone.r, Bone.g, Bone.b, static_cast<std::uint8_t>(alpha * 255.f)));
        mesh.ring(dot, radius, 3.f, Ink);
    }
}

void seatDots(Mesh& mesh, int count, sf::Vector2f c, float radius, float gap)
{
    for (int k = 0; k < count; ++k)
    {
        const sf::Vector2f dot(c.x + (static_cast<float>(k) - static_cast<float>(count - 1) / 2.f) * gap, c.y);
        mesh.circle(dot, radius, Bone);
        mesh.ring(dot, radius, radius * 0.35f, Ink);
    }
}

void ghost(Mesh& mesh, sf::Vector2f c, float size, sf::Color edge)
{
    const float k = size / 100.f;
    auto at = [&](float x, float y) { return sf::Vector2f(c.x + (x - 50.f) * k, c.y + (y - 56.f) * k); };
    const sf::Color fill(0, 0, 0, 46); // black at 18 %
    const float width = 3.f * k;
    const float dash = 7.f * k;
    const float gap = 6.f * k;
    auto part = [&](const std::vector<sf::Vector2f>& points) {
        mesh.polygon(points, fill);
        dashedStroke(mesh, points, width, dash, gap, edge);
    };
    part(ellipsePoints(at(25, 54), 7.f * k, 11.f * k, 0.f, Mesh::segmentsFor(11.f * k)));
    part(ellipsePoints(at(75, 54), 7.f * k, 11.f * k, 0.f, Mesh::segmentsFor(11.f * k)));
    part(ellipsePoints(at(50, 56), 27.f * k, 27.f * k, 0.f, Mesh::segmentsFor(27.f * k)));
    part(roundedRectPoints(at(32, 80).x, at(32, 80).y, 12.f * k, 14.f * k, 4.f * k, 4));
    part(roundedRectPoints(at(56, 80).x, at(56, 80).y, 12.f * k, 14.f * k, 4.f * k, 4));
}

} // namespace MenuArt

} // namespace Bomberman

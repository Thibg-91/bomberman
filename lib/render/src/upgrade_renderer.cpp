#include "upgrade_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

// The item group of the mock-up: translate(9 5) scale(.82) inside the cell.
constexpr sf::Vector2f ItemOffset{9.f, 5.f};
constexpr float ItemScale = 0.82f;

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color Shadow(0, 0, 0, 77);
constexpr sf::Color Twinkle(255, 248, 224);

// ExtraBomb
constexpr sf::Color BombBody(45, 45, 53);
constexpr sf::Color BombShine(255, 255, 255, 71);
constexpr sf::Color Fuse(90, 58, 30);
constexpr sf::Color Spark(255, 207, 77);
constexpr sf::Color Badge(242, 201, 76);

// PushStrength
constexpr sf::Color Skin(219, 163, 107);
constexpr sf::Color Wristband(195, 59, 59);
// Opaque: ink at 50 % over the skin. A translucent stroke would show darker
// dots where its segments and round joins overlap.
constexpr sf::Color Knuckle(124, 92, 60);
constexpr sf::Color Tattoo(44, 74, 102);

// BlastRadius
constexpr sf::Color BurstOuter(242, 145, 58);
constexpr sf::Color BurstInner(255, 215, 94);
constexpr sf::Color BurstCore(255, 255, 255, 217);

// Points of the mock-up's burst(): a star alternating outer and inner radii,
// its first spike pointing up (then turned by rotation radians).
std::vector<sf::Vector2f> burst(sf::Vector2f center, int spikes, float outer, float inner, float rotation = 0.f)
{
    std::vector<sf::Vector2f> points;
    for (int i = 0; i < spikes * 2; ++i)
    {
        const float a = rotation + static_cast<float>(i) * Pi / static_cast<float>(spikes) - Pi / 2.f;
        const float r = (i % 2) ? inner : outer;
        points.push_back({center.x + std::cos(a) * r, center.y + std::sin(a) * r});
    }
    return points;
}

std::vector<sf::Vector2f> rotateAround(std::vector<sf::Vector2f> points, sf::Vector2f pivot, float degrees)
{
    const float a = degrees * Pi / 180.f;
    const float c = std::cos(a);
    const float s = std::sin(a);
    for (sf::Vector2f& point : points)
    {
        const sf::Vector2f d = point - pivot;
        point = {pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c};
    }
    return points;
}

// SVG path of the upper arm: M20 74 Q20 52 40 50 Q58 46 66 62 Q68 76 54 77
// L24 79 Q20 79 20 74 Z.
std::vector<sf::Vector2f> upperArm()
{
    std::vector<sf::Vector2f> points;
    // Each curve after the first starts where the previous one ended:
    // skip that duplicate point.
    const auto append = [&points](const std::vector<sf::Vector2f>& curve, bool continues) {
        points.insert(points.end(), curve.begin() + (continues ? 1 : 0), curve.end());
    };
    append(quadraticBezier({20.f, 74.f}, {20.f, 52.f}, {40.f, 50.f}, 12), false);
    append(quadraticBezier({40.f, 50.f}, {58.f, 46.f}, {66.f, 62.f}, 12), true);
    append(quadraticBezier({66.f, 62.f}, {68.f, 76.f}, {54.f, 77.f}, 10), true);
    // L24 79: a straight edge, then the last curve back to the start.
    append(quadraticBezier({24.f, 79.f}, {20.f, 79.f}, {20.f, 74.f}, 6), false);
    points.pop_back(); // back on the first point: the polygon closes itself
    return points;
}

// Maps mock-up coordinates (view box units) to pixels, with SVG-like fill +
// centered stroke helpers.
class Painter
{
public:
    Painter(Mesh& mesh, sf::Vector2f origin, float unit)
        : m_mesh(mesh),
          m_origin(origin),
          m_unit(unit)
    {
    }

    [[nodiscard]] sf::Vector2f px(sf::Vector2f mock) const { return m_origin + mock * m_unit; }

    [[nodiscard]] std::vector<sf::Vector2f> px(const std::vector<sf::Vector2f>& mock) const
    {
        std::vector<sf::Vector2f> points;
        points.reserve(mock.size());
        for (const sf::Vector2f p : mock)
            points.push_back(px(p));
        return points;
    }

    void ellipse(sf::Vector2f center, float rx, float ry, sf::Color fill, float rotationDeg = 0.f)
    {
        m_mesh.ellipse(px(center), rx * m_unit, ry * m_unit, fill, rotationDeg * Pi / 180.f);
    }

    void outlinedEllipse(sf::Vector2f center, float rx, float ry, sf::Color fill, float stroke,
                         float rotationDeg = 0.f)
    {
        const float w = strokeWidth(stroke);
        m_mesh.ellipse(px(center), rx * m_unit + w / 2.f, ry * m_unit + w / 2.f, Ink, rotationDeg * Pi / 180.f);
        m_mesh.ellipse(px(center), rx * m_unit - w / 2.f, ry * m_unit - w / 2.f, fill, rotationDeg * Pi / 180.f);
    }

    // Any simple polygon, filled then stroked on its edges (round joins).
    void outlinedShape(const std::vector<sf::Vector2f>& mock, sf::Color fill, float stroke)
    {
        const std::vector<sf::Vector2f> points = px(mock);
        m_mesh.fillPolygon(points, fill);
        m_mesh.stroke(points, strokeWidth(stroke), Ink);
    }

    // Polygon star-shaped around center (the bursts).
    void star(sf::Vector2f center, const std::vector<sf::Vector2f>& mock, sf::Color fill)
    {
        m_mesh.fan(px(center), px(mock), fill);
    }

    void rect(float x, float y, float w, float h, float radius, sf::Color fill)
    {
        m_mesh.polygon(px(roundedRectPoints(x, y, w, h, radius)), fill);
    }

    void line(sf::Vector2f a, sf::Vector2f b, float stroke, sf::Color color)
    {
        m_mesh.line(px(a), px(b), strokeWidth(stroke), color, true);
    }

    // Open quadratic curve with round caps.
    void curve(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, float stroke, sf::Color color)
    {
        m_mesh.polyline(px(quadraticBezier(p0, p1, p2, 10)), strokeWidth(stroke), color, true);
    }

    void ring(sf::Vector2f center, float r, float stroke, sf::Color color)
    {
        m_mesh.ring(px(center), r * m_unit, strokeWidth(stroke), color);
    }

private:
    [[nodiscard]] float strokeWidth(float stroke) const { return std::max(1.f, stroke * m_unit); }

    Mesh& m_mesh;
    sf::Vector2f m_origin;
    float m_unit;
};

// Golden "+" badge: plus(cx, cy, r) of the mock-up.
void paintPlusBadge(Painter& p, sf::Vector2f c, float r)
{
    p.outlinedEllipse(c, r, r, Badge, 2.6f);
    p.rect(c.x - r * 0.55f, c.y - r * 0.15f, r * 1.1f, r * 0.3f, 1.f, Ink);
    p.rect(c.x - r * 0.15f, c.y - r * 0.55f, r * 0.3f, r * 1.1f, 1.f, Ink);
}

void paintExtraBomb(Painter& p)
{
    p.outlinedEllipse({46.f, 55.f}, 21.f, 21.f, BombBody, 3.f);
    p.ellipse({39.f, 47.f}, 6.f, 4.5f, BombShine, -30.f);
    p.curve({56.f, 37.f}, {60.f, 31.f}, {62.f, 27.f}, 4.f, Fuse);
    p.star({63.f, 25.f}, burst({63.f, 25.f}, 4, 7.f, 2.6f), Spark);
    paintPlusBadge(p, {70.f, 66.f}, 11.f);
}

void paintPushStrength(Painter& p)
{
    // Forearm, then the upper arm over it, the wristband and the fist.
    p.outlinedEllipse({63.f, 47.f}, 9.f, 18.f, Skin, 3.f, 14.f);
    p.outlinedShape(upperArm(), Skin, 3.f);
    p.outlinedShape(rotateAround(roundedRectPoints(56.f, 34.f, 18.f, 6.f, 2.f), {65.f, 37.f}, 14.f), Wristband, 2.2f);
    p.outlinedEllipse({68.f, 27.f}, 11.f, 11.f, Skin, 3.f);
    p.curve({61.f, 23.f}, {65.f, 21.f}, {69.f, 23.f}, 1.8f, Knuckle);
    p.curve({62.f, 29.f}, {66.f, 27.f}, {70.f, 29.f}, 1.8f, Knuckle);

    // Anchor tattoo on the biceps.
    p.line({40.f, 57.f}, {40.f, 68.f}, 1.8f, Tattoo);
    p.line({36.5f, 59.5f}, {43.5f, 59.5f}, 1.8f, Tattoo);
    p.curve({35.f, 65.f}, {40.f, 71.f}, {45.f, 65.f}, 1.8f, Tattoo);
    p.ring({40.f, 55.5f}, 1.6f, 1.8f, Tattoo);
}

void paintBlastRadius(Painter& p)
{
    constexpr sf::Vector2f Center{50.f, 51.f};
    p.outlinedShape(burst(Center, 12, 35.f, 21.f), BurstOuter, 3.f);
    p.star(Center, burst(Center, 12, 22.f, 13.f, Pi / 12.f), BurstInner);
    p.ellipse(Center, 7.f, 7.f, BurstCore);
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
UpgradeRenderer::UpgradeRenderer(float tileSize)
    : m_tileSize(tileSize),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void UpgradeRenderer::update(const std::vector<Upgrade>& upgrades, float deltaSeconds)
{
    if (deltaSeconds > 0.f)
        m_elapsed = std::fmod(m_elapsed + deltaSeconds, AnimationPeriod);

    // Smooth 0 -> 1 -> 0 over a period, like the mock-up's ease-in-out.
    const float wave = 0.5f - 0.5f * std::cos(m_elapsed / AnimationPeriod * 2.f * Pi);
    const float bob = wave * BobAmplitude;
    const float tileUnit = m_tileSize / 100.f;

    m_vertices.clear();
    Mesh mesh(m_vertices);
    for (const Upgrade& upgrade : upgrades)
    {
        const sf::Vector2f cellOrigin(static_cast<float>(upgrade.position.x) * m_tileSize,
                                      static_cast<float>(upgrade.position.y) * m_tileSize);

        // Socle: the shadow stays on the floor, the star twinkles above.
        Painter cell(mesh, cellOrigin, tileUnit);
        cell.ellipse({50.f, 86.f}, 22.f, 5.5f, Shadow);

        Painter item(mesh, cellOrigin + (ItemOffset - sf::Vector2f(0.f, bob)) * tileUnit, tileUnit * ItemScale);
        switch (upgrade.type)
        {
        case UpgradeType::ExtraBomb:
            paintExtraBomb(item);
            break;
        case UpgradeType::PushStrength:
            paintPushStrength(item);
            break;
        case UpgradeType::BlastRadius:
            paintBlastRadius(item);
            break;
        }

        // The star grows from 60 % to 100 % and fades in with the bob.
        constexpr sf::Vector2f StarCenter{78.f, 20.f};
        const float starScale = 0.6f + 0.4f * wave;
        sf::Color twinkle = Twinkle;
        twinkle.a = static_cast<std::uint8_t>(std::lround((0.2f + 0.8f * wave) * 255.f));
        cell.star(StarCenter, burst(StarCenter, 4, 6.f * starScale, 1.6f * starScale), twinkle);
    }
}

float UpgradeRenderer::getTileSize() const noexcept
{
    return m_tileSize;
}

float UpgradeRenderer::getElapsed() const noexcept
{
    return m_elapsed;
}

float UpgradeRenderer::getBobOffset() const noexcept
{
    return (0.5f - 0.5f * std::cos(m_elapsed / AnimationPeriod * 2.f * Pi)) * BobAmplitude;
}

const sf::VertexArray& UpgradeRenderer::getVertices() const noexcept
{
    return m_vertices;
}

// =========================
// Private Methods
// =========================
void UpgradeRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

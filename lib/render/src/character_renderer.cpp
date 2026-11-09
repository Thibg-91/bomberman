#include "character_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;
constexpr float HopAmplitude = 0.12f;       // fraction of a tile
constexpr float BodyRadiusFraction = 0.33f; // fraction of a tile
constexpr float LegStepAmplitude = 0.09f;   // fraction of a tile
constexpr float MaxRaiseAngleDeg = 140.f;   // arms swing up from the shoulders
constexpr float TailFlapFrequency = 4.f;    // wiggles per cell moved
constexpr float TailFlapAmplitudeDeg = 14.f;

// Mock-up space: 100x100 view box, body circle centered on (50, 56), radius 27.
constexpr float MockBodyRadius = 27.f;
constexpr sf::Vector2f MockBodyCenter{50.f, 56.f};

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color EarInner(201, 138, 90, 179);
constexpr sf::Color BandanaDot(255, 243, 223, 217);
constexpr sf::Color Blush(240, 132, 138, 140);
constexpr sf::Color ScarColor(176, 90, 82);
constexpr sf::Color SparkColor(255, 207, 77);

// Stroke widths of the mock-up, in mock units.
constexpr float BodyStroke = 3.f;
constexpr float BandanaStroke = 2.5f;

sf::Vector2f cellCenter(Position position, float tileSize)
{
    return {(static_cast<float>(position.x) + 0.5f) * tileSize, (static_cast<float>(position.y) + 0.5f) * tileSize};
}

sf::Vector2f directionVector(Direction direction)
{
    const Position offset = step({0, 0}, direction);
    return {static_cast<float>(offset.x), static_cast<float>(offset.y)};
}

sf::Vector2f rotateAround(sf::Vector2f point, sf::Vector2f pivot, float degrees)
{
    const float a = degrees * Pi / 180.f;
    const float c = std::cos(a);
    const float s = std::sin(a);
    const sf::Vector2f d = point - pivot;
    return {pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c};
}

std::vector<sf::Vector2f> rotateAround(std::vector<sf::Vector2f> points, sf::Vector2f pivot, float degrees)
{
    for (sf::Vector2f& point : points)
        point = rotateAround(point, pivot, degrees);
    return points;
}

// SVG "M a Q c1 b Q c2 a Z": a closed shape made of two quadratic curves.
std::vector<sf::Vector2f> twoCurveLoop(sf::Vector2f a, sf::Vector2f c1, sf::Vector2f b, sf::Vector2f c2)
{
    std::vector<sf::Vector2f> points = quadraticBezier(a, c1, b, 12);
    const std::vector<sf::Vector2f> back = quadraticBezier(b, c2, a, 12);
    points.insert(points.end(), back.begin() + 1, back.end() - 1);
    return points;
}

// Upper half of the bandana circle: from (22, 50) over the top to (78, 50).
std::vector<sf::Vector2f> bandanaDome()
{
    constexpr int Segments = 32;
    std::vector<sf::Vector2f> points;
    for (int i = 0; i <= Segments; ++i)
    {
        const float a = Pi + Pi * static_cast<float>(i) / static_cast<float>(Segments);
        points.push_back({50.f + 28.f * std::cos(a), 50.f + 28.f * std::sin(a)});
    }
    return points;
}

// The dome closed by the pointed fabric edge low on the forehead.
std::vector<sf::Vector2f> bandanaFront()
{
    static constexpr std::array<sf::Vector2f, 9> Trim = {
        sf::Vector2f{72.f, 48.f}, sf::Vector2f{66.f, 54.f}, sf::Vector2f{60.f, 47.f},
        sf::Vector2f{54.f, 53.f}, sf::Vector2f{50.f, 46.f}, sf::Vector2f{46.f, 53.f},
        sf::Vector2f{40.f, 47.f}, sf::Vector2f{34.f, 54.f}, sf::Vector2f{28.f, 48.f}};
    std::vector<sf::Vector2f> points = bandanaDome();
    points.insert(points.end(), Trim.begin(), Trim.end());
    return points;
}

// Seen from behind, the bandana wraps the lower back of the head too.
std::vector<sf::Vector2f> bandanaBackBand()
{
    std::vector<sf::Vector2f> points = quadraticBezier({23.f, 48.f}, {50.f, 70.f}, {77.f, 48.f}, 16);
    const std::vector<sf::Vector2f> lower = quadraticBezier({77.f, 58.f}, {50.f, 76.f}, {23.f, 58.f}, 16);
    points.insert(points.end(), lower.begin(), lower.end());
    return points;
}

// Draws mock-up coordinates as pixels, with SVG-like fill + stroke helpers.
// A mirrored painter flips the mock-up horizontally (right profile).
class Painter
{
public:
    Painter(Mesh& mesh, sf::Vector2f bodyCenter, float unit, bool mirrored)
        : m_mesh(mesh),
          m_bodyCenter(bodyCenter),
          m_unit(unit),
          m_mirrored(mirrored)
    {
    }

    [[nodiscard]] sf::Vector2f px(sf::Vector2f mock) const
    {
        const float x = m_mirrored ? 100.f - mock.x : mock.x;
        return m_bodyCenter + (sf::Vector2f{x, mock.y} - MockBodyCenter) * m_unit;
    }

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
        m_mesh.ellipse(px(center), rx * m_unit, ry * m_unit, fill, angle(rotationDeg));
    }

    void outlinedEllipse(sf::Vector2f center, float rx, float ry, sf::Color fill, float stroke, float rotationDeg = 0.f)
    {
        const float w = strokeWidth(stroke);
        m_mesh.ellipse(px(center), rx * m_unit + w / 2.f, ry * m_unit + w / 2.f, Ink, angle(rotationDeg));
        m_mesh.ellipse(px(center), rx * m_unit - w / 2.f, ry * m_unit - w / 2.f, fill, angle(rotationDeg));
    }

    void outlinedShape(const std::vector<sf::Vector2f>& mock, sf::Color fill, float stroke)
    {
        const std::vector<sf::Vector2f> points = px(mock);
        m_mesh.fillPolygon(points, fill);
        m_mesh.stroke(points, strokeWidth(stroke), Ink);
    }

    void roundedRect(float x, float y, float w, float h, float radius, sf::Color fill)
    {
        const sf::Vector2f a = px({x, y});
        const sf::Vector2f b = px({x + w, y + h});
        m_mesh.polygon(roundedRectPoints(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x),
                                         std::abs(b.y - a.y), radius * m_unit),
                       fill);
    }

    void line(sf::Vector2f a, sf::Vector2f b, float stroke, sf::Color color, bool roundCaps)
    {
        m_mesh.line(px(a), px(b), strokeWidth(stroke), color, roundCaps);
    }

    // Open quadratic curve with round caps (smiles).
    void curve(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, float stroke, sf::Color color)
    {
        m_mesh.polyline(px(quadraticBezier(p0, p1, p2, 12)), strokeWidth(stroke), color, true);
    }

private:
    [[nodiscard]] float strokeWidth(float stroke) const { return std::max(1.f, stroke * m_unit); }
    [[nodiscard]] float angle(float degrees) const { return (m_mirrored ? -degrees : degrees) * Pi / 180.f; }

    Mesh& m_mesh;
    sf::Vector2f m_bodyCenter;
    float m_unit;
    bool m_mirrored;
};

// Animated offsets, in mock units / degrees.
struct Pose
{
    float leftLift = 0.f;
    float rightLift = 0.f;
    float armAngle = 0.f;
    float armNudge = 0.f;
    float tailAngle = 0.f;
};

void paintLegs(Painter& p, const Pose& pose, bool single)
{
    if (single)
    {
        p.roundedRect(44.f, 80.f - std::max(pose.leftLift, pose.rightLift), 12.f, 14.f, 4.f, Ink);
        return;
    }
    p.roundedRect(32.f, 80.f - pose.leftLift, 12.f, 14.f, 4.f, Ink);
    p.roundedRect(56.f, 80.f - pose.rightLift, 12.f, 14.f, 4.f, Ink);
}

void paintArms(Painter& p, const Pose& pose, bool single, sf::Color skin)
{
    if (single)
    {
        p.outlinedEllipse({50.f, 60.f - pose.armNudge}, 7.f, 10.f, skin, BodyStroke);
        return;
    }
    // For the bomb-drop pose, each arm swings up around its shoulder.
    constexpr sf::Vector2f LeftShoulder{29.f, 47.f};
    constexpr sf::Vector2f RightShoulder{71.f, 47.f};
    p.outlinedEllipse(rotateAround({25.f, 54.f}, LeftShoulder, pose.armAngle), 7.f, 11.f, skin, BodyStroke,
                      pose.armAngle);
    p.outlinedEllipse(rotateAround({75.f, 54.f}, RightShoulder, -pose.armAngle), 7.f, 11.f, skin, BodyStroke,
                      -pose.armAngle);
}

void paintEar(Painter& p, sf::Color skin)
{
    p.outlinedEllipse({50.f, 56.f}, 4.2f, 5.2f, skin, BandanaStroke);
    p.ellipse({50.f, 56.5f}, 1.9f, 2.6f, EarInner);
}

void paintKnotAndTails(Painter& p, sf::Color bandana, sf::Vector2f knot, float knotRadius,
                       const std::vector<sf::Vector2f>& tailA, const std::vector<sf::Vector2f>& tailB, float tailAngle)
{
    p.outlinedEllipse(knot, knotRadius, knotRadius, bandana, BandanaStroke);
    p.outlinedShape(rotateAround(tailA, knot, tailAngle), bandana, BandanaStroke);
    p.outlinedShape(rotateAround(tailB, knot, -tailAngle * 0.7f), bandana, BandanaStroke);
}

void paintFrontDots(Painter& p)
{
    for (const sf::Vector2f dot : {sf::Vector2f{38.f, 42.f}, sf::Vector2f{50.f, 38.f}, sf::Vector2f{62.f, 42.f}})
        p.ellipse(dot, 2.1f, 2.1f, BandanaDot);
}

void paintFront(Painter& p, const CharacterRenderer::Palette& palette, const Pose& pose)
{
    paintEar(p, palette.skin);
    p.outlinedShape(bandanaFront(), palette.bandana, BandanaStroke);
    paintKnotAndTails(p, palette.bandana, {70.f, 40.f}, 7.f,
                      twoCurveLoop({58.f, 30.f}, {70.f, 26.f}, {70.f, 40.f}, {62.f, 36.f}),
                      twoCurveLoop({64.f, 32.f}, {76.f, 34.f}, {72.f, 47.f}, {65.f, 40.f}), pose.tailAngle);
    paintFrontDots(p);

    p.ellipse({36.f, 62.f}, 5.f, 3.2f, Blush);
    p.ellipse({64.f, 62.f}, 5.f, 3.2f, Blush);
    p.ellipse({41.f, 55.f}, 6.4f, 7.4f, Ink);
    p.ellipse({39.f, 52.f}, 1.6f, 1.6f, sf::Color::White);
    p.curve({46.f, 65.f}, {50.f, 68.f}, {54.f, 65.f}, 2.2f, Ink);
    // Eyepatch strap stops right under the bandana; patch tilted along it.
    p.line({41.f, 48.5f}, {77.f, 65.f}, 2.4f, Ink, false);
    p.ellipse({60.f, 55.f}, 7.4f, 8.4f, Ink, 25.f);
    p.line({34.f, 60.f}, {33.f, 65.f}, 1.6f, ScarColor, true);
}

void paintBack(Painter& p, const CharacterRenderer::Palette& palette, const Pose& pose)
{
    p.outlinedShape(bandanaDome(), palette.bandana, BandanaStroke);
    p.outlinedShape(bandanaBackBand(), palette.bandana, BandanaStroke);
    paintKnotAndTails(p, palette.bandana, {50.f, 40.f}, 8.f,
                      twoCurveLoop({42.f, 46.f}, {32.f, 58.f}, {40.f, 68.f}, {46.f, 56.f}),
                      twoCurveLoop({58.f, 46.f}, {68.f, 58.f}, {60.f, 68.f}, {54.f, 56.f}), pose.tailAngle);
    p.ellipse({36.f, 44.f}, 2.1f, 2.1f, BandanaDot);
    p.ellipse({64.f, 44.f}, 2.1f, 2.1f, BandanaDot);
    p.ellipse({42.f, 62.f}, 2.f, 2.f, BandanaDot);
    p.ellipse({58.f, 62.f}, 2.f, 2.f, BandanaDot);
}

// Authored facing left; the painter mirrors it for the right profile.
void paintProfile(Painter& p, const CharacterRenderer::Palette& palette, const Pose& pose)
{
    paintEar(p, palette.skin);
    p.outlinedShape(bandanaFront(), palette.bandana, BandanaStroke);
    paintKnotAndTails(p, palette.bandana, {74.f, 44.f}, 7.f,
                      twoCurveLoop({66.f, 36.f}, {80.f, 32.f}, {80.f, 46.f}, {70.f, 42.f}),
                      twoCurveLoop({70.f, 40.f}, {84.f, 44.f}, {78.f, 56.f}, {68.f, 50.f}), pose.tailAngle);
    paintFrontDots(p);

    p.ellipse({26.f, 62.f}, 5.f, 3.2f, Blush);
    p.ellipse({30.f, 55.f}, 6.8f, 7.6f, Ink);
    p.ellipse({28.f, 52.f}, 1.7f, 1.7f, sf::Color::White);
    p.curve({25.f, 65.f}, {30.f, 68.f}, {34.f, 66.f}, 2.2f, Ink);
    p.line({20.f, 58.f}, {18.f, 64.f}, 1.6f, ScarColor, true);
}

} // namespace

CharacterRenderer::Palette::Palette() noexcept : skin(219, 163, 107), bandana(195, 59, 59)
{
}

// =========================
// Static Methods
// =========================
CharacterRenderer::Palette CharacterRenderer::paletteForPlayer(int playerIndex) noexcept
{
    static const std::array<Palette, PlayerCount> palettes = {
        Palette{sf::Color(219, 163, 107), sf::Color(195, 59, 59)},  // red
        Palette{sf::Color(219, 163, 107), sf::Color(47, 111, 174)}, // blue
        Palette{sf::Color(219, 163, 107), sf::Color(58, 138, 78)},  // green
        Palette{sf::Color(219, 163, 107), sf::Color(201, 146, 47)}, // gold
    };
    return palettes[static_cast<std::size_t>(std::clamp(playerIndex, 0, PlayerCount - 1))];
}

// =========================
// Constructors / Destructor
// =========================
CharacterRenderer::CharacterRenderer(float tileSize, Palette palette)
    : m_tileSize(tileSize),
      m_palette(palette),
      m_center(0.f, 0.f),
      m_bodyCenter(0.f, 0.f),
      m_bombAnimElapsed(BombAnimDuration),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void CharacterRenderer::update(const Character& character, float deltaSeconds)
{
    const float progress = character.getMoveProgress();
    const sf::Vector2f from = cellCenter(character.getPreviousPosition(), m_tileSize);
    const sf::Vector2f to = cellCenter(character.getPosition(), m_tileSize);
    m_center = from + (to - from) * progress;

    // Hop while walking, flat when idle (progress is 1 then).
    const float hop = std::sin(progress * Pi) * HopAmplitude * m_tileSize;
    m_bodyCenter = {m_center.x, m_center.y - hop};

    if (deltaSeconds > 0.f && m_bombAnimElapsed < BombAnimDuration)
        m_bombAnimElapsed = std::min(m_bombAnimElapsed + deltaSeconds, BombAnimDuration);
    const float bombWave = isPlayingBombAnimation() ? std::sin((m_bombAnimElapsed / BombAnimDuration) * Pi) : 0.f;

    const float unit = m_tileSize * BodyRadiusFraction / MockBodyRadius;
    const float liftRange = LegStepAmplitude * m_tileSize / unit;

    // Legs: one lifts, then the other, once per cell crossed; both rest flat
    // when idle. Arms only move for the bomb-drop pose.
    Pose pose;
    pose.leftLift = (progress < 0.5f ? std::sin(progress / 0.5f * Pi) : 0.f) * liftRange;
    pose.rightLift = (progress >= 0.5f ? std::sin((progress - 0.5f) / 0.5f * Pi) : 0.f) * liftRange;
    pose.armAngle = bombWave * MaxRaiseAngleDeg;
    pose.armNudge = bombWave * 8.f;
    pose.tailAngle = character.isMoving() ? std::sin(progress * TailFlapFrequency * Pi) * TailFlapAmplitudeDeg : 0.f;

    m_vertices.clear();
    Mesh mesh(m_vertices);
    const Direction facing = character.getFacing();

    // Ground spark where the bomb lands, under everything else.
    if (bombWave > 0.f)
    {
        sf::Color spark = SparkColor;
        spark.a = static_cast<std::uint8_t>(bombWave * 230.f);
        const sf::Vector2f sparkPos =
            m_center + directionVector(facing) * (m_tileSize * 0.55f) + sf::Vector2f(0.f, m_tileSize * 0.12f);
        const float grow = 1.f + bombWave * 0.4f;
        mesh.ellipse(sparkPos, m_tileSize * 0.12f * grow, m_tileSize * 0.05f * grow, spark);
    }

    const bool profile = facing == Direction::Left || facing == Direction::Right;
    Painter painter(mesh, m_bodyCenter, unit, facing == Direction::Right);
    paintLegs(painter, pose, profile);
    paintArms(painter, pose, profile, m_palette.skin);
    painter.outlinedEllipse(MockBodyCenter, MockBodyRadius, MockBodyRadius, m_palette.skin, BodyStroke);

    switch (facing)
    {
    case Direction::Down:
        paintFront(painter, m_palette, pose);
        break;
    case Direction::Up:
        paintBack(painter, m_palette, pose);
        break;
    case Direction::Left:
    case Direction::Right:
        paintProfile(painter, m_palette, pose);
        break;
    }
}

void CharacterRenderer::triggerBombAnimation() noexcept
{
    m_bombAnimElapsed = 0.f;
}

bool CharacterRenderer::isPlayingBombAnimation() const noexcept
{
    return m_bombAnimElapsed < BombAnimDuration;
}

sf::Vector2f CharacterRenderer::getCenter() const noexcept
{
    return m_center;
}

sf::Vector2f CharacterRenderer::getBodyCenter() const noexcept
{
    return m_bodyCenter;
}

const sf::VertexArray& CharacterRenderer::getVertices() const noexcept
{
    return m_vertices;
}

// =========================
// Private Methods
// =========================
void CharacterRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

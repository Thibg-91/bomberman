#include "character_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
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

    // Any simple polygon, filled without outline.
    void fillShape(const std::vector<sf::Vector2f>& mock, sf::Color fill)
    {
        m_mesh.fillPolygon(px(mock), fill);
    }

    // Open path with round caps and joins (SVG stroke-linecap/linejoin round).
    void strokePath(const std::vector<sf::Vector2f>& mock, float stroke, sf::Color color)
    {
        m_mesh.polyline(px(mock), strokeWidth(stroke), color, true);
    }

    void outlinedRoundedRect(float x, float y, float w, float h, float radius, sf::Color fill, float stroke)
    {
        outlinedShape(roundedRectPoints(x, y, w, h, radius), fill, stroke);
    }

    void ring(sf::Vector2f center, float r, float stroke, sf::Color color)
    {
        m_mesh.ring(px(center), r * m_unit, strokeWidth(stroke), color);
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

void paintLegs(Painter& p, const Pose& pose, bool single, sf::Color color = Ink)
{
    if (single)
    {
        p.roundedRect(44.f, 80.f - std::max(pose.leftLift, pose.rightLift), 12.f, 14.f, 4.f, color);
        return;
    }
    p.roundedRect(32.f, 80.f - pose.leftLift, 12.f, 14.f, 4.f, color);
    p.roundedRect(56.f, 80.f - pose.rightLift, 12.f, 14.f, 4.f, color);
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

void paintFrontDots(Painter& p, sf::Color color = BandanaDot)
{
    for (const sf::Vector2f dot : {sf::Vector2f{38.f, 42.f}, sf::Vector2f{50.f, 38.f}, sf::Vector2f{62.f, 42.f}})
        p.ellipse(dot, 2.1f, 2.1f, color);
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


// =========================
// Other crew models (mock-up "Équipage du galion")
// =========================
constexpr sf::Color White(255, 255, 255);

// SVG path made of quadratic segments: start, then (control, end) pairs. A
// straight "L" segment is a quadratic whose control is its midpoint.
struct Quad
{
    sf::Vector2f control;
    sf::Vector2f to;
};

std::vector<sf::Vector2f> path(sf::Vector2f start, std::initializer_list<Quad> quads, bool closed = true)
{
    std::vector<sf::Vector2f> points = {start};
    sf::Vector2f from = start;
    for (const Quad& quad : quads)
    {
        const std::vector<sf::Vector2f> curve = quadraticBezier(from, quad.control, quad.to, 10);
        points.insert(points.end(), curve.begin() + 1, curve.end());
        from = quad.to;
    }
    // A closed path ending on its start: drop the duplicate point.
    if (closed && points.size() > 1 && std::hypot(points.back().x - start.x, points.back().y - start.y) < 1e-3f)
        points.pop_back();
    return points;
}

sf::Vector2f mid(sf::Vector2f a, sf::Vector2f b)
{
    return (a + b) / 2.f;
}

// eye(x, y, rx, ry) of the mock-up: a black oval with a white highlight.
void paintEye(Painter& p, sf::Vector2f c, float rx = 5.5f, float ry = 6.5f)
{
    p.ellipse(c, rx, ry, Ink);
    p.ellipse({c.x - rx * 0.3f, c.y - ry * 0.4f}, std::max(1.4f, rx * 0.28f), std::max(1.4f, rx * 0.28f), White);
}

void paintBlush(Painter& p, sf::Vector2f c)
{
    p.ellipse(c, 5.f, 3.2f, Blush);
}

// ---------- Captain ----------
constexpr sf::Color Hat(42, 42, 51);
constexpr sf::Color Gold(212, 169, 74);
constexpr sf::Color Mustache(74, 44, 20);
constexpr sf::Color Bone(243, 234, 220);

void paintSkullEmblem(Painter& p, sf::Vector2f c)
{
    p.outlinedEllipse(c, 4.6f, 4.6f, Bone, 1.4f);
    p.ellipse({c.x - 1.7f, c.y - 0.6f}, 1.1f, 1.1f, Ink);
    p.ellipse({c.x + 1.7f, c.y - 0.6f}, 1.1f, 1.1f, Ink);
    p.line({c.x - 7.f, c.y + 3.f}, {c.x + 7.f, c.y + 8.f}, 2.2f, Bone, true);
    p.line({c.x + 7.f, c.y + 3.f}, {c.x - 7.f, c.y + 8.f}, 2.2f, Bone, true);
}

void paintMustacheHalf(Painter& p, float side) // side: -1 left, +1 right
{
    const auto at = [side](float dx, float y) { return sf::Vector2f{50.f + side * dx, y}; };
    p.outlinedShape(path(at(0.f, 63.f), {{at(8.f, 59.f), at(15.f, 65.f)},
                                         {at(17.f, 59.f), at(12.f, 59.f)},
                                         {at(6.f, 58.f), at(0.f, 61.f)}}),
                    Mustache, 1.8f);
}

void paintCaptainCrown(Painter& p, float dx)
{
    p.outlinedShape(path({27.f + dx, 41.f}, {{{28.f + dx, 13.f}, {50.f + dx, 13.f}}, {{72.f + dx, 13.f}, {73.f + dx, 41.f}}}),
                    Hat, BodyStroke);
}

void paintCaptainFront(Painter& p)
{
    paintBlush(p, {35.f, 63.f});
    paintBlush(p, {65.f, 63.f});
    paintEye(p, {41.f, 54.f});
    paintEye(p, {59.f, 54.f});
    paintMustacheHalf(p, -1.f);
    paintMustacheHalf(p, 1.f);
    p.curve({46.f, 69.f}, {50.f, 71.f}, {54.f, 69.f}, 2.2f, Ink);
    paintCaptainCrown(p, 0.f);
    p.outlinedShape(path({8.f, 26.f}, {{{34.f, 38.f}, {50.f, 52.f}},
                                       {{66.f, 38.f}, {92.f, 26.f}},
                                       {{74.f, 37.f}, {50.f, 36.f}},
                                       {{26.f, 37.f}, {8.f, 26.f}}}),
                    Hat, BodyStroke);
    p.strokePath(path({13.f, 29.5f}, {{{35.f, 39.f}, {50.f, 48.5f}}, {{65.f, 39.f}, {87.f, 29.5f}}}, false), 2.4f, Gold);
    paintSkullEmblem(p, {50.f, 24.f});
}

void paintCaptainBack(Painter& p)
{
    p.outlinedShape(path({26.f, 44.f}, {{{50.f, 60.f}, {74.f, 44.f}},
                                        {mid({74.f, 44.f}, {73.f, 52.f}), {73.f, 52.f}},
                                        {{50.f, 68.f}, {27.f, 52.f}},
                                        {mid({27.f, 52.f}, {26.f, 44.f}), {26.f, 44.f}}}),
                    Mustache, 2.4f);
    paintCaptainCrown(p, 0.f);
    p.outlinedShape(path({8.f, 28.f}, {{{30.f, 38.f}, {50.f, 38.f}},
                                       {{70.f, 38.f}, {92.f, 28.f}},
                                       {{74.f, 48.f}, {50.f, 49.f}},
                                       {{26.f, 48.f}, {8.f, 28.f}}}),
                    Hat, BodyStroke);
    p.strokePath(path({13.f, 32.f}, {{{32.f, 46.f}, {50.f, 47.f}}, {{68.f, 46.f}, {87.f, 32.f}}}, false), 2.4f, Gold);
}

void paintCaptainProfile(Painter& p, sf::Color skin)
{
    paintEar(p, skin);
    paintBlush(p, {27.f, 63.f});
    paintEye(p, {31.f, 54.f});
    p.outlinedShape(path({31.f, 63.f}, {{{24.f, 58.f}, {17.f, 64.f}},
                                        {{13.f, 62.f}, {15.f, 58.f}},
                                        {{19.f, 55.f}, {23.f, 58.f}},
                                        {{27.f, 58.f}, {31.f, 61.f}}}),
                    Mustache, 1.8f);
    p.curve({26.f, 69.f}, {30.f, 71.f}, {34.f, 69.f}, 2.2f, Ink);
    p.outlinedShape(path({30.f, 41.f}, {{{31.f, 13.f}, {53.f, 13.f}}, {{74.f, 13.f}, {74.f, 41.f}}}), Hat, BodyStroke);
    p.outlinedShape(path({12.f, 30.f}, {{{24.f, 38.f}, {40.f, 46.f}},
                                        {{64.f, 42.f}, {88.f, 32.f}},
                                        {{72.f, 38.f}, {52.f, 37.f}},
                                        {{30.f, 37.f}, {12.f, 30.f}}}),
                    Hat, BodyStroke);
    p.strokePath(path({18.f, 33.5f}, {{{27.f, 39.f}, {40.f, 43.f}}, {{62.f, 40.f}, {83.f, 34.f}}}, false), 2.4f, Gold);
    paintSkullEmblem(p, {46.f, 26.f});
}

// ---------- Lookout ----------
constexpr sf::Color Knit(47, 111, 174);
constexpr sf::Color KnitStripe(243, 234, 220);
constexpr sf::Color Pompom(195, 59, 59);
// Thin strokes are made of overlapping segments and round joins: a
// translucent color would show darker dots where they overlap. They use the
// opaque color the mock-up's translucent ink gives over their background.
constexpr sf::Color Rib(189, 181, 168); // ink at 25 % over the cuff
constexpr sf::Color Freckle(181, 112, 63);
constexpr sf::Color OpenMouth(122, 42, 42);

void paintBeanie(Painter& p, float dx)
{
    p.outlinedShape(path({22.f + dx, 46.f}, {{{22.f + dx, 16.f}, {50.f + dx, 16.f}}, {{78.f + dx, 16.f}, {78.f + dx, 46.f}}}),
                    Knit, BodyStroke);
    p.curve({25.f + dx, 36.f}, {50.f + dx, 31.f}, {75.f + dx, 36.f}, 4.5f, KnitStripe);
    p.curve({31.f + dx, 26.f}, {50.f + dx, 22.f}, {69.f + dx, 26.f}, 4.5f, KnitStripe);
    p.outlinedRoundedRect(19.f + dx, 41.f, 62.f, 11.f, 5.5f, KnitStripe, BodyStroke);
    for (float x = 29.f; x <= 69.f; x += 8.f)
        p.line({x + dx, 44.f}, {x + dx, 49.f}, 1.6f, Rib, true);
    p.outlinedEllipse({50.f + dx, 13.f}, 7.5f, 7.5f, Pompom, 2.6f);
}

void paintFreckles(Painter& p, std::initializer_list<sf::Vector2f> dots)
{
    for (const sf::Vector2f dot : dots)
        p.ellipse(dot, 1.1f, 1.1f, Freckle);
}

void paintLookoutFront(Painter& p)
{
    paintBlush(p, {35.f, 66.f});
    paintBlush(p, {65.f, 66.f});
    paintEye(p, {40.f, 60.f}, 6.f, 7.f);
    paintEye(p, {60.f, 60.f}, 6.f, 7.f);
    paintFreckles(p, {{31.f, 63.f}, {34.f, 66.f}, {30.f, 67.f}, {69.f, 63.f}, {66.f, 66.f}, {70.f, 67.f}});
    p.outlinedShape(path({44.f, 68.f}, {{{50.f, 76.f}, {56.f, 68.f}}}), OpenMouth, 2.f);
    paintBeanie(p, 0.f);
}

void paintLookoutProfile(Painter& p, sf::Color skin)
{
    paintEar(p, skin);
    paintBlush(p, {27.f, 66.f});
    paintEye(p, {31.f, 60.f}, 6.f, 7.f);
    paintFreckles(p, {{26.f, 64.f}, {29.f, 67.f}, {25.f, 68.f}});
    p.outlinedShape(path({24.f, 68.f}, {{{29.f, 75.f}, {34.f, 69.f}}}), OpenMouth, 2.f);
    paintBeanie(p, 3.f);
}

// ---------- Parrot ----------
constexpr sf::Color Feathers(63, 163, 77);
constexpr sf::Color Belly(167, 211, 107);
constexpr sf::Color Beak(242, 201, 76);
constexpr sf::Color Crest(216, 67, 47);
constexpr sf::Color Feet(224, 138, 46);
constexpr sf::Color TailBlue(47, 111, 174);
constexpr sf::Color FeatherLine(54, 127, 61); // ink at 25 % over the feathers
constexpr sf::Color BeakLine(146, 120, 48);    // ink at 45 % over the beak

void paintCrest(Painter& p, float dx)
{
    p.outlinedEllipse({43.f + dx, 31.f}, 4.2f, 10.f, Crest, 2.4f, -28.f);
    p.outlinedEllipse({50.f + dx, 27.f}, 4.2f, 10.f, Crest, 2.4f);
    p.outlinedEllipse({57.f + dx, 31.f}, 4.2f, 10.f, Crest, 2.4f, 28.f);
}

void paintParrotEye(Painter& p, sf::Vector2f c, sf::Vector2f pupil)
{
    p.outlinedEllipse(c, 6.5f, 6.5f, White, 2.2f);
    p.ellipse(pupil, 3.2f, 3.2f, Ink);
}

// Drawn before the arms (tail feathers) and before the body (crest).
void paintParrotTail(Painter& p, Direction facing)
{
    if (facing == Direction::Up)
    {
        p.outlinedEllipse({44.f, 82.f}, 5.f, 12.f, TailBlue, 2.4f, 18.f);
        p.outlinedEllipse({56.f, 82.f}, 5.f, 12.f, Crest, 2.4f, -18.f);
    }
    else if (facing != Direction::Down)
    {
        p.outlinedEllipse({74.f, 80.f}, 5.f, 12.f, TailBlue, 2.4f, -40.f);
    }
}

void paintParrotFace(Painter& p, Direction facing)
{
    switch (facing)
    {
    case Direction::Down:
        p.ellipse({50.f, 67.f}, 16.f, 13.f, Belly);
        paintParrotEye(p, {38.f, 48.f}, {39.f, 49.f});
        paintParrotEye(p, {62.f, 48.f}, {61.f, 49.f});
        p.outlinedShape(path({43.f, 55.f}, {{{50.f, 50.f}, {57.f, 55.f}}, {{56.f, 63.f}, {50.f, 68.f}}, {{44.f, 63.f}, {43.f, 55.f}}}),
                        Beak, 2.4f);
        p.curve({46.f, 61.f}, {50.f, 64.f}, {54.f, 61.f}, 1.6f, BeakLine);
        break;
    case Direction::Up:
        p.curve({36.f, 44.f}, {50.f, 38.f}, {64.f, 44.f}, 2.f, FeatherLine);
        p.curve({38.f, 54.f}, {50.f, 49.f}, {62.f, 54.f}, 2.f, FeatherLine);
        break;
    case Direction::Left:
    case Direction::Right:
        p.ellipse({40.f, 67.f}, 11.f, 12.f, Belly);
        paintParrotEye(p, {36.f, 47.f}, {34.f, 48.f});
        p.outlinedShape(path({27.f, 51.f}, {{{16.f, 49.f}, {13.f, 58.f}}, {{16.f, 64.f}, {22.f, 61.f}}, {{24.f, 57.f}, {29.f, 58.f}}}),
                        Beak, 2.4f);
        break;
    }
}

// ---------- Revenant ----------
constexpr sf::Color Skeleton(236, 230, 214);
constexpr sf::Color DarkBandana(58, 45, 74);
constexpr sf::Color Glow(123, 224, 160);
constexpr sf::Color SkeletonDot(236, 230, 214, 217);
constexpr sf::Color Crack(122, 115, 103);     // ink at 55 % over the bone
constexpr sf::Color BackCrack(132, 125, 114); // ink at 50 % over the bone

void paintSocket(Painter& p, sf::Vector2f c)
{
    p.ellipse(c, 7.f, 8.f, Ink);
    p.ellipse({c.x, c.y + 1.f}, 2.2f, 2.2f, Glow);
}

void paintTeeth(Painter& p, float x, float w)
{
    p.outlinedRoundedRect(x, 69.f, w, 7.f, 2.f, White, 2.f);
    const int gaps = static_cast<int>(std::lround(w / 5.f));
    for (int i = 1; i < gaps; ++i)
    {
        const float lx = x + static_cast<float>(i) * w / static_cast<float>(gaps);
        p.line({lx, 69.5f}, {lx, 75.5f}, 1.4f, Ink, false);
    }
}

void paintRevenantFront(Painter& p, const Pose& pose)
{
    p.outlinedShape(bandanaFront(), DarkBandana, BandanaStroke);
    paintKnotAndTails(p, DarkBandana, {70.f, 40.f}, 7.f,
                      twoCurveLoop({58.f, 30.f}, {70.f, 26.f}, {70.f, 40.f}, {62.f, 36.f}),
                      twoCurveLoop({64.f, 32.f}, {76.f, 34.f}, {72.f, 47.f}, {65.f, 40.f}), pose.tailAngle);
    paintFrontDots(p, SkeletonDot);
    paintSocket(p, {40.f, 59.f});
    paintSocket(p, {60.f, 59.f});
    p.fillShape({{50.f, 63.f}, {47.5f, 66.5f}, {52.5f, 66.5f}}, Ink);
    paintTeeth(p, 40.f, 20.f);
    p.strokePath({{73.f, 52.f}, {69.f, 56.f}, {72.f, 59.f}, {68.f, 63.f}}, 1.6f, Crack);
}

void paintRevenantBack(Painter& p, const Pose& pose)
{
    p.outlinedShape(bandanaDome(), DarkBandana, BandanaStroke);
    p.outlinedShape(bandanaBackBand(), DarkBandana, BandanaStroke);
    paintKnotAndTails(p, DarkBandana, {50.f, 40.f}, 8.f,
                      twoCurveLoop({42.f, 46.f}, {32.f, 58.f}, {40.f, 68.f}, {46.f, 56.f}),
                      twoCurveLoop({58.f, 46.f}, {68.f, 58.f}, {60.f, 68.f}, {54.f, 56.f}), pose.tailAngle);
    p.strokePath({{36.f, 66.f}, {40.f, 70.f}, {37.f, 73.f}}, 1.6f, BackCrack);
}

void paintRevenantProfile(Painter& p, const Pose& pose)
{
    p.outlinedShape(bandanaFront(), DarkBandana, BandanaStroke);
    paintKnotAndTails(p, DarkBandana, {74.f, 44.f}, 7.f,
                      twoCurveLoop({66.f, 36.f}, {80.f, 32.f}, {80.f, 46.f}, {70.f, 42.f}),
                      twoCurveLoop({70.f, 40.f}, {84.f, 44.f}, {78.f, 56.f}, {68.f, 50.f}), pose.tailAngle);
    paintFrontDots(p, SkeletonDot);
    paintSocket(p, {31.f, 59.f});
    p.fillShape({{23.f, 64.f}, {20.5f, 67.f}, {24.5f, 67.5f}}, Ink);
    paintTeeth(p, 22.f, 14.f);
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
CharacterRenderer::CharacterRenderer(float tileSize, Palette palette, CharacterModel model)
    : m_tileSize(tileSize),
      m_palette(palette),
      m_model(model),
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

    // Shared template: legs, (tail), arms, (crest), body, then the model's
    // head and face.
    const bool parrot = m_model == CharacterModel::Parrot;
    const sf::Color bodyColor = parrot                                  ? Feathers
                              : m_model == CharacterModel::Revenant ? Skeleton
                                                                    : m_palette.skin;
    paintLegs(painter, pose, profile, parrot ? Feet : Ink);
    if (parrot)
        paintParrotTail(painter, facing);
    paintArms(painter, pose, profile, bodyColor);
    if (parrot)
        paintCrest(painter, profile ? 4.f : 0.f);
    painter.outlinedEllipse(MockBodyCenter, MockBodyRadius, MockBodyRadius, bodyColor, BodyStroke);

    switch (m_model)
    {
    case CharacterModel::Buccaneer:
        if (facing == Direction::Down)
            paintFront(painter, m_palette, pose);
        else if (facing == Direction::Up)
            paintBack(painter, m_palette, pose);
        else
            paintProfile(painter, m_palette, pose);
        break;
    case CharacterModel::Captain:
        if (facing == Direction::Down)
            paintCaptainFront(painter);
        else if (facing == Direction::Up)
            paintCaptainBack(painter);
        else
            paintCaptainProfile(painter, m_palette.skin);
        break;
    case CharacterModel::Lookout:
        if (facing == Direction::Down)
            paintLookoutFront(painter);
        else if (facing == Direction::Up)
            paintBeanie(painter, 0.f);
        else
            paintLookoutProfile(painter, m_palette.skin);
        break;
    case CharacterModel::Parrot:
        paintParrotFace(painter, facing);
        break;
    case CharacterModel::Revenant:
        if (facing == Direction::Down)
            paintRevenantFront(painter, pose);
        else if (facing == Direction::Up)
            paintRevenantBack(painter, pose);
        else
            paintRevenantProfile(painter, pose);
        break;
    }

    if (m_brightness < 1.f)
    {
        const auto dim = [this](std::uint8_t v) {
            return static_cast<std::uint8_t>(std::lround(static_cast<float>(v) * m_brightness));
        };
        for (std::size_t i = 0; i < m_vertices.getVertexCount(); ++i)
        {
            sf::Color& c = m_vertices[i].color;
            c = sf::Color(dim(c.r), dim(c.g), dim(c.b), c.a);
        }
    }

    if (m_deathElapsed >= 0.f)
    {
        if (deltaSeconds > 0.f)
            m_deathElapsed = std::min(m_deathElapsed + deltaSeconds, DeathDuration);
        applyDeathAnimation();
    }
}

CharacterModel CharacterRenderer::getModel() const noexcept
{
    return m_model;
}

void CharacterRenderer::setModel(CharacterModel model) noexcept
{
    m_model = model;
}

void CharacterRenderer::setBrightness(float brightness) noexcept
{
    m_brightness = std::clamp(brightness, 0.f, 1.f);
}

float CharacterRenderer::getBrightness() const noexcept
{
    return m_brightness;
}

void CharacterRenderer::triggerBombAnimation() noexcept
{
    m_bombAnimElapsed = 0.f;
}

bool CharacterRenderer::isPlayingBombAnimation() const noexcept
{
    return m_bombAnimElapsed < BombAnimDuration;
}

void CharacterRenderer::triggerDeathAnimation() noexcept
{
    if (m_deathElapsed < 0.f)
        m_deathElapsed = 0.f;
}

bool CharacterRenderer::isDying() const noexcept
{
    return m_deathElapsed >= 0.f && m_deathElapsed < DeathDuration;
}

bool CharacterRenderer::isDeathAnimationFinished() const noexcept
{
    return m_deathElapsed >= DeathDuration;
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
void CharacterRenderer::applyDeathAnimation()
{
    // Timings and offsets of the mock-up, whose vignette has 120-unit cells.
    const float t = m_deathElapsed;
    const float k = m_tileSize / 120.f;
    const auto span = [t](float from, float to) { return std::clamp((t - from) / (to - from), 0.f, 1.f); };
    const auto lerp = [](float a, float b, float u) { return a + (b - a) * u; };
    // Same pseudo-random sequence as the mock-up's hash(i, salt).
    const auto hash = [](int i, int salt) {
        const double x = std::sin(i * 127.1 + salt * 311.7) * 43758.5453;
        return static_cast<float>(x - std::floor(x));
    };

    const float unit = m_tileSize * BodyRadiusFraction / MockBodyRadius;
    const sf::Vector2f soles = m_bodyCenter + sf::Vector2f(0.f, (94.f - MockBodyCenter.y) * unit);
    const float charring = span(0.12f, 0.35f);
    const float crumble = span(0.6f, 1.1f);

    // 1. The character: white flash, charred, shaking, then squashed down
    // onto its soles while it fades.
    if (crumble >= 1.f)
        m_vertices.clear();
    else
    {
        const float light = t < 0.12f ? 2.2f : lerp(1.f, 0.18f, charring);
        const float jitter = (t > 0.2f && t < 0.6f) ? std::sin(t * 90.f) * m_tileSize * 0.03f : 0.f;
        const float stretch = lerp(1.f, 1.15f, crumble);
        const float height = 1.f - crumble;
        const float opacity = 1.f - crumble * 0.6f;
        const auto scaled = [light](std::uint8_t v) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(static_cast<float>(v) * light), 0L, 255L));
        };
        for (std::size_t i = 0; i < m_vertices.getVertexCount(); ++i)
        {
            sf::Vertex& v = m_vertices[i];
            v.position = {soles.x + jitter + (v.position.x - soles.x) * stretch,
                          soles.y + (v.position.y - soles.y) * height};
            v.color = sf::Color(scaled(v.color.r), scaled(v.color.g), scaled(v.color.b),
                                static_cast<std::uint8_t>(std::lround(static_cast<float>(v.color.a) * opacity)));
        }
    }

    Mesh mesh(m_vertices);

    // 2. Ashes scattering and falling.
    if (crumble > 0.f)
    {
        const auto alpha = static_cast<std::uint8_t>(std::lround(255.f * (1.f - span(1.0f, 1.3f))));
        for (int i = 0; i < 10; ++i)
        {
            const float angle = Pi * (1.05f + hash(i, 1) * 0.9f);
            const float distance = (20.f + hash(i, 2) * 60.f) * crumble;
            const float fall = crumble * crumble * 40.f;
            const sf::Vector2f at = soles + sf::Vector2f(std::cos(angle) * distance * 1.6f,
                                                         -6.f + std::sin(angle) * distance * 0.5f + fall) * k;
            const sf::Color ash = i % 2 ? sf::Color(43, 38, 34, alpha) : sf::Color(74, 68, 62, alpha);
            mesh.circle(at, (4.f + hash(i, 3) * 5.f) * k, ash);
        }
    }

    // 3. A puff of grey smoke rising.
    const float smoke = span(0.55f, 1.3f);
    if (smoke > 0.f && smoke < 1.f)
    {
        const sf::Vector2f center = m_bodyCenter + sf::Vector2f(0.f, 10.f - smoke * 40.f) * k;
        const float r = (40.f + 40.f * smoke) * k;
        const sf::Color grey(138, 133, 128, static_cast<std::uint8_t>(std::lround(255.f * 0.8f * (1.f - smoke))));
        for (const sf::Vector3f puff : {sf::Vector3f(-0.5f, 0.1f, 0.55f), sf::Vector3f(0.f, -0.2f, 0.7f),
                                        sf::Vector3f(0.5f, 0.1f, 0.55f)})
            mesh.circle(center + sf::Vector2f(puff.x, puff.y) * r, puff.z * r, grey);
    }
}

void CharacterRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

#ifndef MESH_H
#define MESH_H

#pragma once

#include <vector>

#include <SFML/Graphics.hpp>

namespace Bomberman
{

// Shape tessellation shared by the renderers: every shape is appended as
// triangles to an sf::VertexArray. Building it needs no GL context (geometry
// is unit-testable headless) and drawing it is a single call. Strokes behave
// like SVG ones: centered on the path, with round joins, so thin or pointy
// shapes never get the miter spikes of sf::Shape outlines.
class Mesh
{
public:
    explicit Mesh(sf::VertexArray& vertices) noexcept;

    // Number of segments that keeps a circle of this radius (pixels) smooth.
    [[nodiscard]] static int segmentsFor(float radius) noexcept;

    void triangle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color ca, sf::Color cb, sf::Color cc);
    void triangle(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Color color);
    void quad(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3, sf::Color color);
    void rect(float x, float y, float w, float h, sf::Color color);

    // Convex polygon.
    void polygon(const std::vector<sf::Vector2f>& points, sf::Color color);
    // Polygon star-shaped around center (not necessarily convex).
    void fan(sf::Vector2f center, const std::vector<sf::Vector2f>& points, sf::Color color);
    // Any simple (non self-intersecting) polygon, convex or not.
    void fillPolygon(const std::vector<sf::Vector2f>& points, sf::Color color);

    // segments <= 0 picks segmentsFor() the largest radius.
    void ellipse(sf::Vector2f center, float rx, float ry, sf::Color color, float rotation = 0.f, int segments = 0);
    void circle(sf::Vector2f center, float r, sf::Color color, int segments = 0);
    void ring(sf::Vector2f center, float r, float width, sf::Color color, int segments = 0);

    // Disc sector fading from color at the center to transparent at the rim.
    void glowSector(sf::Vector2f center, float r, sf::Color color, float from, float to, int segments = 24);

    void line(sf::Vector2f a, sf::Vector2f b, float width, sf::Color color, bool roundCaps = false);
    void polyline(const std::vector<sf::Vector2f>& points, float width, sf::Color color, bool roundJoins = false);
    // Closed outline, centered on the polygon edges, with round joins.
    void stroke(const std::vector<sf::Vector2f>& points, float width, sf::Color color);
    void arc(sf::Vector2f center, float r, float from, float to, float width, sf::Color color, int segments = 10);

private:
    sf::VertexArray& m_vertices;
};

[[nodiscard]] std::vector<sf::Vector2f> ellipsePoints(sf::Vector2f center, float rx, float ry, float rotation,
                                                      int segments);
[[nodiscard]] std::vector<sf::Vector2f> roundedRectPoints(float x, float y, float w, float h, float r,
                                                          int perCorner = 4);
// Points of a quadratic Bezier curve, both ends included.
[[nodiscard]] std::vector<sf::Vector2f> quadraticBezier(sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2,
                                                        int segments);

} // namespace Bomberman

#endif // MESH_H

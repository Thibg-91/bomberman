#include "explosion_renderer.h"

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

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color FlameRed(216, 67, 47);
constexpr sf::Color FlameOuter(242, 145, 58);
constexpr sf::Color FlameInner(255, 215, 94);
constexpr sf::Color FlameCore(255, 243, 196);
constexpr sf::Color SmokeOuter(141, 138, 134);
constexpr sf::Color SmokeInner(185, 181, 174);

float clamp01(float v)
{
    return std::clamp(v, 0.f, 1.f);
}

float easeOut(float t)
{
    return 1.f - std::pow(1.f - t, 3.f);
}

sf::Color mix(sf::Color from, sf::Color to, float t)
{
    const auto channel = [t](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(a) * (1.f - t) + static_cast<float>(b) * t));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b)};
}

sf::Vector2f directionVector(Direction direction)
{
    const Position offset = step({0, 0}, direction);
    return {static_cast<float>(offset.x), static_cast<float>(offset.y)};
}

// Mock-up timeline, u in [0, 1] over the explosion.
float opacityAt(float u)
{
    return u < 0.6f ? 1.f : clamp01(1.f - (u - 0.6f) / 0.4f);
}

// Fireballs: the center ball, then on every arm a small ball between two
// cells and a ball on each cell, shrinking with the distance. Each pops
// 0.05 s of timeline per cell after the previous one (ease-out with a small
// overshoot) and turns to smoke from u = 0.55.
void paintFireballs(Mesh& mesh, const Blast& blast, sf::Vector2f center, float unit, float u)
{
    struct Ball
    {
        sf::Vector2f position; // mock units from the center
        float radius;
        float distance; // cells from the center, for the pop delay
    };
    std::vector<Ball> balls = {{{0.f, 0.f}, 44.f, 0.f}};
    for (std::size_t arm = 0; arm < Blast::Directions.size(); ++arm)
    {
        const sf::Vector2f d = directionVector(Blast::Directions[arm]);
        for (int i = 1; i <= blast.reach[arm]; ++i)
        {
            const float k = static_cast<float>(i);
            balls.push_back({d * ((k - 0.5f) * 100.f), 24.f, k - 0.5f});
            balls.push_back({d * (k * 100.f), std::max(38.f - 3.f * k, 12.f), k});
        }
    }

    const float smoke = clamp01((u - 0.55f) / 0.45f);
    const auto scaleOf = [u](float distance) {
        const float v = clamp01((u - distance * 0.05f) / 0.18f);
        return v <= 0.f ? 0.f : easeOut(v) * (1.f + 0.15f * std::sin(v * Pi));
    };

    // Layer by layer over every ball, so the ink outlines the whole chain.
    struct Layer
    {
        sf::Color color;
        float scale;
        float outline; // mock units added to the radius
        float lift;    // fraction of the radius the layer is raised by
    };
    const std::array<Layer, 3> layers = {Layer{Ink, 1.f, 3.f, 0.f},
                                         Layer{mix(FlameOuter, SmokeOuter, smoke), 0.92f, 0.f, 0.f},
                                         Layer{mix(FlameInner, SmokeInner, smoke), 0.58f, 0.f, -0.18f}};
    for (const Layer& layer : layers)
    {
        for (const Ball& ball : balls)
        {
            const float s = scaleOf(ball.distance);
            if (s <= 0.f)
                continue;
            const float radius = (ball.radius * layer.scale + layer.outline) * s;
            const sf::Vector2f position = ball.position + sf::Vector2f(0.f, layer.lift * ball.radius * s);
            mesh.circle(center + position * unit, radius * unit, layer.color);
        }
    }
}

// Flame cross: bands with round ends from the center along each arm,
// shooting out over the first 0.25 of the timeline and thinning while
// fading out; one ink outline under the whole cross.
void paintFlameCross(Mesh& mesh, const Blast& blast, sf::Vector2f center, float unit, float u)
{
    const float grow = easeOut(clamp01(u / 0.25f));
    const float thin = u < 0.6f ? 1.f : 0.55f + 0.45f * opacityAt(u);

    struct Layer
    {
        sf::Color color;
        float width; // mock units
    };
    const std::array<Layer, 5> layers = {Layer{Ink, 82.f}, Layer{FlameRed, 76.f}, Layer{FlameOuter, 56.f},
                                         Layer{FlameInner, 34.f}, Layer{FlameCore, 14.f}};
    for (const Layer& layer : layers)
    {
        const float width = std::max(1.f, layer.width * thin * unit);
        for (std::size_t arm = 0; arm < Blast::Directions.size(); ++arm)
        {
            const float length = static_cast<float>(blast.reach[arm]) * 100.f * grow;
            if (length <= 0.f)
                continue;
            const sf::Vector2f end = center + directionVector(Blast::Directions[arm]) * (length * unit);
            mesh.line(center, end, width, layer.color, true);
        }
        mesh.circle(center, (layer.width / 2.f + 12.f) * thin * (0.6f + 0.4f * grow) * unit, layer.color);
    }
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
ExplosionRenderer::ExplosionRenderer(float tileSize, Style style)
    : m_tileSize(tileSize),
      m_style(style),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void ExplosionRenderer::add(const Blast& blast)
{
    m_explosions.push_back(Active{blast});
}

void ExplosionRenderer::update(float deltaSeconds)
{
    for (Active& explosion : m_explosions)
        explosion.elapsed += std::max(deltaSeconds, 0.f);
    m_explosions.erase(std::remove_if(m_explosions.begin(), m_explosions.end(),
                                      [](const Active& explosion) { return explosion.elapsed >= Duration; }),
                       m_explosions.end());

    const float unit = m_tileSize / 100.f;
    m_vertices.clear();
    Mesh mesh(m_vertices);
    for (Active& explosion : m_explosions)
    {
        const float u = clamp01(explosion.elapsed / Duration);
        const sf::Vector2f center((static_cast<float>(explosion.blast.center.x) + 0.5f) * m_tileSize,
                                  (static_cast<float>(explosion.blast.center.y) + 0.5f) * m_tileSize);
        explosion.firstVertex = m_vertices.getVertexCount();
        if (m_style == Style::Fireballs)
            paintFireballs(mesh, explosion.blast, center, unit, u);
        else
            paintFlameCross(mesh, explosion.blast, center, unit, u);
        explosion.vertexCount = m_vertices.getVertexCount() - explosion.firstVertex;
        explosion.opacity = opacityAt(u);
    }
}

ExplosionRenderer::Style ExplosionRenderer::getStyle() const noexcept
{
    return m_style;
}

float ExplosionRenderer::getTileSize() const noexcept
{
    return m_tileSize;
}

std::size_t ExplosionRenderer::getActiveCount() const noexcept
{
    return m_explosions.size();
}

float ExplosionRenderer::getOpacity(std::size_t index) const
{
    return m_explosions.at(index).opacity;
}

const sf::VertexArray& ExplosionRenderer::getVertices() const noexcept
{
    return m_vertices;
}

// =========================
// Private Methods
// =========================
void ExplosionRenderer::drawFadedVertices(sf::RenderTarget& target, const sf::RenderStates& states,
                                          const sf::Vertex* vertices, std::size_t count, float opacity)
{
    std::vector<sf::Vertex> faded(vertices, vertices + count);
    for (sf::Vertex& vertex : faded)
        vertex.color.a = static_cast<std::uint8_t>(std::lround(static_cast<float>(vertex.color.a) * opacity));
    target.draw(faded.data(), faded.size(), sf::PrimitiveType::Triangles, states);
}

void ExplosionRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    for (const Active& explosion : m_explosions)
    {
        if (explosion.vertexCount == 0)
            continue;
        const sf::Vertex* vertices = &m_vertices[explosion.firstVertex];
        if (explosion.opacity >= 1.f)
        {
            target.draw(vertices, explosion.vertexCount, sf::PrimitiveType::Triangles, states);
            continue;
        }

        // Fading: render the explosion opaque off-screen, then blend the
        // result as a whole.
        sf::Vector2f min = vertices[0].position;
        sf::Vector2f max = min;
        for (std::size_t i = 1; i < explosion.vertexCount; ++i)
        {
            min.x = std::min(min.x, vertices[i].position.x);
            min.y = std::min(min.y, vertices[i].position.y);
            max.x = std::max(max.x, vertices[i].position.x);
            max.y = std::max(max.y, vertices[i].position.y);
        }
        min = {std::floor(min.x) - 1.f, std::floor(min.y) - 1.f};
        const sf::Vector2u size(static_cast<unsigned>(std::ceil(max.x - min.x)) + 2u,
                                static_cast<unsigned>(std::ceil(max.y - min.y)) + 2u);

        if (!m_canvas)
            m_canvas = std::make_unique<sf::RenderTexture>();
        const sf::Vector2u canvasSize = m_canvas->getSize();
        if (canvasSize.x < size.x || canvasSize.y < size.y)
        {
            // Off-screen textures may support less anti-aliasing than the
            // window (4x on some Macs): asking for more makes resize() fail.
            sf::ContextSettings settings;
            settings.antiAliasingLevel = std::min(8u, sf::RenderTexture::getMaximumAntiAliasingLevel());
            if (!m_canvas->resize({std::max(canvasSize.x, size.x), std::max(canvasSize.y, size.y)}, settings))
            {
                // No off-screen texture at all: fade each vertex instead (the
                // outlines show through a little, but the explosion stays).
                drawFadedVertices(target, states, vertices, explosion.vertexCount, explosion.opacity);
                continue;
            }
        }

        m_canvas->setView(sf::View(sf::FloatRect(min, sf::Vector2f(size))));
        m_canvas->clear(sf::Color::Transparent);
        m_canvas->draw(vertices, explosion.vertexCount, sf::PrimitiveType::Triangles);
        m_canvas->display();

        sf::Sprite sprite(m_canvas->getTexture(), sf::IntRect({0, 0}, sf::Vector2i(size)));
        sprite.setPosition(min);
        sprite.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(std::lround(explosion.opacity * 255.f))));
        target.draw(sprite, states);
    }
}

} // namespace Bomberman

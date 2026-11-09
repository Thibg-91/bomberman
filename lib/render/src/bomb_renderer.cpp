#include "bomb_renderer.h"

#include <algorithm>
#include <cmath>

#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float PulseAmplitude = 0.08f;       // fraction of body scale
constexpr float SlowestBlinkPeriod = 0.5f;    // seconds, right after the fuse is lit
constexpr float FastestBlinkPeriod = 0.08f;   // seconds, right before exploding

sf::Vector2f cellCenter(Position position, float tileSize)
{
    return {(static_cast<float>(position.x) + 0.5f) * tileSize, (static_cast<float>(position.y) + 0.5f) * tileSize};
}

float angleDegrees(sf::Vector2f from, sf::Vector2f to)
{
    return std::atan2(to.y - from.y, to.x - from.x) * 180.f / 3.14159265f;
}

float lengthOf(sf::Vector2f from, sf::Vector2f to)
{
    return std::hypot(to.x - from.x, to.y - from.y);
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
BombRenderer::BombRenderer(float tileSize, sf::Color color)
    : m_tileSize(tileSize),
      m_center(0.f, 0.f),
      m_body(tileSize * 0.33f),
      m_spark(tileSize * 0.07f)
{
    // SFML's default 30 sides look faceted at fullscreen tile sizes.
    m_body.setPointCount(static_cast<std::size_t>(Mesh::segmentsFor(m_body.getRadius())));
    m_spark.setPointCount(static_cast<std::size_t>(Mesh::segmentsFor(m_spark.getRadius())));
    m_body.setFillColor(color);
    m_body.setOutlineColor(sf::Color::Black);
    m_body.setOutlineThickness(std::max(1.f, tileSize * 0.045f));
    m_body.setOrigin({m_body.getRadius(), m_body.getRadius()});

    m_fuse.setFillColor(sf::Color(90, 58, 30));
    m_fuse.setOrigin({0.f, tileSize * 0.025f});

    m_spark.setFillColor(sf::Color(255, 207, 77));
    m_spark.setOrigin({m_spark.getRadius(), m_spark.getRadius()});
}

// =========================
// Public Methods
// =========================
void BombRenderer::update(const Bomb& bomb)
{
    m_center = cellCenter(bomb.getPosition(), m_tileSize);
    m_body.setPosition(m_center);

    // 0 right after the bomb is dropped, 1 right before it explodes.
    const float progress = bomb.getFuseDuration() > 0.f
        ? 1.f - std::clamp(bomb.getTimeRemaining() / bomb.getFuseDuration(), 0.f, 1.f)
        : 1.f;
    const float elapsed = bomb.getFuseDuration() - bomb.getTimeRemaining();

    // The body pulses faster and faster as the fuse runs out.
    const float pulseFrequency = 3.f + progress * 9.f;
    const float scale = 1.f + std::sin(elapsed * pulseFrequency) * PulseAmplitude;
    m_body.setScale({scale, scale});

    // The spark on the fuse blinks, speeding up towards the explosion.
    const float blinkPeriod = std::max(SlowestBlinkPeriod - progress * (SlowestBlinkPeriod - FastestBlinkPeriod),
                                        FastestBlinkPeriod);
    const float blinkPhase = std::fmod(elapsed, blinkPeriod);
    const bool sparkVisible = blinkPhase < blinkPeriod * 0.5f;

    const sf::Vector2f fuseBase = m_center + sf::Vector2f(m_body.getRadius() * 0.3f, -m_body.getRadius() * 0.95f);
    const sf::Vector2f fuseTip = m_center + sf::Vector2f(m_body.getRadius() * 0.5f, -m_body.getRadius() * 1.4f);
    m_fuse.setSize({lengthOf(fuseBase, fuseTip), m_tileSize * 0.05f});
    m_fuse.setPosition(fuseBase);
    m_fuse.setRotation(sf::degrees(angleDegrees(fuseBase, fuseTip)));
    m_spark.setPosition(fuseTip);
    sf::Color sparkColor = m_spark.getFillColor();
    sparkColor.a = sparkVisible ? 255 : 60;
    m_spark.setFillColor(sparkColor);
}

sf::Vector2f BombRenderer::getCenter() const noexcept
{
    return m_center;
}

const sf::CircleShape& BombRenderer::getBody() const noexcept
{
    return m_body;
}

const sf::CircleShape& BombRenderer::getSpark() const noexcept
{
    return m_spark;
}

const sf::RectangleShape& BombRenderer::getFuse() const noexcept
{
    return m_fuse;
}

// =========================
// Private Methods
// =========================
void BombRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_body, states);
    target.draw(m_fuse, states);
    target.draw(m_spark, states);
}

} // namespace Bomberman

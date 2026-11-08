#include "bomb_renderer.h"

#include <algorithm>
#include <cmath>

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
    m_body.setFillColor(color);
    m_body.setOutlineColor(sf::Color::Black);
    m_body.setOutlineThickness(2.f);
    m_body.setOrigin({m_body.getRadius(), m_body.getRadius()});

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

    const sf::Vector2f fuseTip = m_center + sf::Vector2f(m_body.getRadius() * 0.5f, -m_body.getRadius() * 1.4f);
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

// =========================
// Private Methods
// =========================
void BombRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_body, states);
    target.draw(m_spark, states);
}

} // namespace Bomberman

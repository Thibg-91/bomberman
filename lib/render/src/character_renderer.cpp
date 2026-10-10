#include "character_renderer.h"

#include <cmath>

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;
constexpr float HopAmplitude = 0.12f; // fraction of a tile

sf::Vector2f cellCenter(Position position, float tileSize)
{
    return {(static_cast<float>(position.x) + 0.5f) * tileSize, (static_cast<float>(position.y) + 0.5f) * tileSize};
}

sf::Vector2f directionVector(Direction direction)
{
    const Position offset = step({0, 0}, direction);
    return {static_cast<float>(offset.x), static_cast<float>(offset.y)};
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
CharacterRenderer::CharacterRenderer(float tileSize, sf::Color color)
    : m_tileSize(tileSize),
      m_center(0.f, 0.f),
      m_body(tileSize * 0.38f),
      m_eye(tileSize * 0.09f)
{
    m_body.setFillColor(color);
    m_body.setOutlineColor(sf::Color::Black);
    m_body.setOutlineThickness(2.f);
    m_body.setOrigin({m_body.getRadius(), m_body.getRadius()});

    m_eye.setFillColor(sf::Color::Black);
    m_eye.setOrigin({m_eye.getRadius(), m_eye.getRadius()});
}

// =========================
// Public Methods
// =========================
void CharacterRenderer::update(const Character& character)
{
    const float progress = character.getMoveProgress();
    const sf::Vector2f from = cellCenter(character.getPreviousPosition(), m_tileSize);
    const sf::Vector2f to = cellCenter(character.getPosition(), m_tileSize);
    m_center = from + (to - from) * progress;

    // Hop while walking, flat when idle (progress is 1 then).
    const float hop = std::sin(progress * Pi) * HopAmplitude * m_tileSize;
    m_body.setPosition({m_center.x, m_center.y - hop});

    // The eye shows where the character is facing.
    m_eye.setPosition(m_body.getPosition() + directionVector(character.getFacing()) * (m_tileSize * 0.2f));
}

sf::Vector2f CharacterRenderer::getCenter() const noexcept
{
    return m_center;
}

const sf::CircleShape& CharacterRenderer::getBody() const noexcept
{
    return m_body;
}

// =========================
// Private Methods
// =========================
void CharacterRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_body, states);
    target.draw(m_eye, states);
}

} // namespace Bomberman

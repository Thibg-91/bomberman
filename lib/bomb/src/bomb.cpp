#include "bomb.h"

#include <algorithm>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Bomb::Bomb(Position position, float fuseDuration, int blastRadius) noexcept
    : m_position(position),
      m_fuseDuration(std::max(fuseDuration, 0.f)),
      m_timeRemaining(m_fuseDuration),
      m_blastRadius(std::max(blastRadius, 0))
{
}

// =========================
// Public Methods
// =========================
Position Bomb::getPosition() const noexcept
{
    return m_position;
}

void Bomb::setPosition(Position position) noexcept
{
    m_position = position;
}

float Bomb::getFuseDuration() const noexcept
{
    return m_fuseDuration;
}

float Bomb::getTimeRemaining() const noexcept
{
    return m_timeRemaining;
}

int Bomb::getBlastRadius() const noexcept
{
    return m_blastRadius;
}

bool Bomb::hasExploded() const noexcept
{
    return m_timeRemaining <= 0.f;
}

void Bomb::update(float deltaSeconds) noexcept
{
    if (!hasExploded() && deltaSeconds > 0.f)
        m_timeRemaining = std::max(m_timeRemaining - deltaSeconds, 0.f);
}

} // namespace Bomberman

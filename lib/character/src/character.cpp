#include "character.h"

#include <algorithm>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Character::Character(Position spawn, float moveDuration) noexcept
    : m_position(spawn),
      m_previousPosition(spawn),
      m_moveDuration(std::max(moveDuration, 0.f)),
      m_moveElapsed(m_moveDuration)
{
}

// =========================
// Public Methods
// =========================
Position Character::getPosition() const noexcept
{
    return m_position;
}

Position Character::getPreviousPosition() const noexcept
{
    return m_previousPosition;
}

Direction Character::getFacing() const noexcept
{
    return m_facing;
}

float Character::getMoveDuration() const noexcept
{
    return m_moveDuration;
}

bool Character::isAlive() const noexcept
{
    return m_alive;
}

void Character::kill() noexcept
{
    m_alive = false;
}

bool Character::isMoving() const noexcept
{
    return m_moveElapsed < m_moveDuration;
}

float Character::getMoveProgress() const noexcept
{
    return isMoving() ? m_moveElapsed / m_moveDuration : 1.f;
}

void Character::update(float deltaSeconds) noexcept
{
    if (isMoving() && deltaSeconds > 0.f)
    {
        m_moveElapsed = std::min(m_moveElapsed + deltaSeconds, m_moveDuration);
        // Summing fixed ticks (e.g. 10 x 1/60 s) can fall a rounding error
        // short of moveDuration: the walk must still end on that tick.
        if (m_moveDuration - m_moveElapsed < TimeTolerance)
            m_moveElapsed = m_moveDuration;
    }
}

bool Character::move(Direction direction, const Grid& grid, const std::vector<Position>& obstacles) noexcept
{
    if (isMoving())
        return false;

    // First press on a new direction: just turn.
    if (direction != m_facing)
    {
        m_facing = direction;
        return false;
    }

    const Position target = step(m_position, direction);
    if (!grid.isWalkable(target) || std::find(obstacles.begin(), obstacles.end(), target) != obstacles.end())
        return false;

    m_previousPosition = m_position;
    m_position = target;
    m_moveElapsed = 0.f;
    return true;
}

bool Character::execute(const CharacterCommand& command, const Grid& grid,
                        const std::vector<Position>& obstacles) noexcept
{
    switch (command.type)
    {
    case CharacterCommand::Type::Move:
        return move(command.direction, grid, obstacles);
    case CharacterCommand::Type::PushBomb:
    case CharacterCommand::Type::PlaceBomb:
        return false;
    }
    return false;
}

} // namespace Bomberman

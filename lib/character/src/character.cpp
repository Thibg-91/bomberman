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
        m_moveElapsed = std::min(m_moveElapsed + deltaSeconds, m_moveDuration);
}

bool Character::move(Direction direction, const Grid& grid) noexcept
{
    if (isMoving())
        return false;

    m_facing = direction;

    const Position target = step(m_position, direction);
    if (!grid.isWalkable(target))
        return false;

    m_previousPosition = m_position;
    m_position = target;
    m_moveElapsed = 0.f;
    return true;
}

bool Character::breakBlock(Grid& grid) noexcept
{
    if (isMoving())
        return false;

    const Position target = step(m_position, m_facing);
    if (!grid.isInside(target))
        return false;

    return grid.getTile(target).destroy();
}

bool Character::execute(const CharacterCommand& command, Grid& grid) noexcept
{
    switch (command.type)
    {
    case CharacterCommand::Type::Move:
        return move(command.direction, grid);
    case CharacterCommand::Type::BreakBlock:
        return breakBlock(grid);
    }
    return false;
}

} // namespace Bomberman

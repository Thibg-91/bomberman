#ifndef CHARACTER_H
#define CHARACTER_H

#pragma once

#include "character_command.h"
#include "direction.h"
#include "grid.h"
#include "position.h"

namespace Bomberman
{

// A player-like entity living on a Grid. It moves cell by cell: the logical
// position changes as soon as a move is accepted, then the character is
// "busy" for moveDuration seconds (see update()) while a renderer can
// interpolate from getPreviousPosition() to getPosition() using
// getMoveProgress(). The class is purely logical (no input, no rendering) and
// is driven through CharacterCommand, so it can be reused by AI or network
// controlled characters.
class Character
{
public:
    static constexpr float DefaultMoveDuration = 0.15f; // seconds per cell

    // =========================
    // Constructors / Destructor
    // =========================
    explicit Character(Position spawn, float moveDuration = DefaultMoveDuration) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] Position getPosition() const noexcept;
    [[nodiscard]] Position getPreviousPosition() const noexcept;
    [[nodiscard]] Direction getFacing() const noexcept;
    [[nodiscard]] float getMoveDuration() const noexcept;

    // True while the last move is still being "walked".
    [[nodiscard]] bool isMoving() const noexcept;
    // 0 right after a move, 1 once finished (always 1 when idle).
    [[nodiscard]] float getMoveProgress() const noexcept;

    // Make time pass (seconds).
    void update(float deltaSeconds) noexcept;

    // Turn towards direction, then move one cell if it is walkable.
    // Ignored (returns false) while the character is still moving.
    bool move(Direction direction, const Grid& grid) noexcept;

    // Destroy the destructible block in the cell the character is facing.
    // Returns true if a block has been destroyed, so that callers know the
    // grid must be redrawn. Ignored while the character is moving.
    bool breakBlock(Grid& grid) noexcept;

    // Dispatch a command; returns whether it had an effect.
    bool execute(const CharacterCommand& command, Grid& grid) noexcept;

private:
    // =========================
    // Members
    // =========================
    Position m_position;
    Position m_previousPosition;
    Direction m_facing = Direction::Down;
    float m_moveDuration;
    float m_moveElapsed;
};

} // namespace Bomberman

#endif // CHARACTER_H

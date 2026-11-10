#ifndef CHARACTER_H
#define CHARACTER_H

#pragma once

#include <vector>

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
    // A walk this close (seconds) to its end counts as finished.
    static constexpr float TimeTolerance = 1e-4f;

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

    // True until the character is caught in an explosion (see kill()).
    [[nodiscard]] bool isAlive() const noexcept;
    void kill() noexcept;

    // True while the last move is still being "walked".
    [[nodiscard]] bool isMoving() const noexcept;
    // 0 right after a move, 1 once finished (always 1 when idle).
    [[nodiscard]] float getMoveProgress() const noexcept;

    // Make time pass (seconds).
    void update(float deltaSeconds) noexcept;

    // Faces direction and moves one cell that way at once if the cell is
    // walkable and not one of the obstacles (cells holding entities that
    // block movement, e.g. bombs: Character does not know about them, the
    // caller lists them; only the target cell is checked, so a character
    // can always step off the bomb it stands on). Towards a blocked cell it
    // only turns. Returns whether the character moved. Ignored (returns
    // false, no turn) while the character is still moving.
    bool move(Direction direction, const Grid& grid, const std::vector<Position>& obstacles = {}) noexcept;

    // Dispatch a command; returns whether it had an effect. Bomb commands
    // (PushBomb, PlaceBomb) need a BombManager, which Character does not know
    // about: they always return false and callers handle them (see
    // exe/main.cpp). A character never modifies the grid. obstacles: see move().
    bool execute(const CharacterCommand& command, const Grid& grid,
                 const std::vector<Position>& obstacles = {}) noexcept;

private:
    // =========================
    // Members
    // =========================
    Position m_position;
    Position m_previousPosition;
    Direction m_facing = Direction::Down;
    float m_moveDuration;
    float m_moveElapsed;
    bool m_alive = true;
};

} // namespace Bomberman

#endif // CHARACTER_H

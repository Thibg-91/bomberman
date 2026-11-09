#ifndef BOMB_MANAGER_H
#define BOMB_MANAGER_H

#pragma once

#include <vector>

#include "bomb.h"
#include "direction.h"
#include "grid.h"
#include "position.h"

namespace Bomberman
{

// Owns every bomb currently on the grid: placement, countdown, explosion
// (destroying destructible blocks in a cross) and being pushed when punched.
// Purely logical, driven the same way as Character: update(dt) advances
// time, the caller decides what to do with the cells hit by an explosion
// (e.g. kill characters standing on them).
class BombManager
{
public:
    static constexpr int DefaultPushDistance = 1;    // cells per punch
    static constexpr int DefaultMaxActiveBombs = 1;  // bombs a character may have on the grid at once

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] const std::vector<Bomb>& getBombs() const noexcept;

    [[nodiscard]] bool hasBombAt(Position position) const noexcept;

    // Bombs dropped by ownerId that have not exploded yet.
    [[nodiscard]] int countActiveBombs(int ownerId) const noexcept;

    // Cells currently holding a bomb, e.g. the obstacles to give to
    // Character::move() so characters cannot walk through bombs.
    [[nodiscard]] std::vector<Position> getBombPositions() const;

    // Drops a bomb owned by ownerId on position. Fails (returns false) if
    // the cell is not walkable (wall/destructible block), already holds a
    // bomb, or ownerId already has maxActiveBombs bombs on the grid (a new
    // one can be dropped once one of them has exploded).
    bool placeBomb(Position position, const Grid& grid, int ownerId = 0,
                   int maxActiveBombs = DefaultMaxActiveBombs);

    // Pushes the bomb standing on position by distance cells in direction.
    // Fails if there is no bomb there, or the destination cell is not
    // walkable or already occupied by another bomb; the bomb then stays put.
    bool pushBombAt(Position position, Direction direction, const Grid& grid,
                    int distance = DefaultPushDistance) noexcept;

    // Advances every bomb's fuse and explodes those reaching zero: the
    // destructible blocks their blast cross reaches are destroyed. Returns
    // every cell touched by an explosion during this call (bomb centers
    // included), so the caller can check it against character positions.
    [[nodiscard]] std::vector<Position> update(float deltaSeconds, Grid& grid);

private:
    // =========================
    // Private Methods
    // =========================
    // Appends the cells destroyed/reached by a bomb exploding at center to
    // hitCells, destroying the destructible blocks it meets along the way.
    void explode(Position center, int blastRadius, Grid& grid, std::vector<Position>& hitCells) const;

    // =========================
    // Members
    // =========================
    std::vector<Bomb> m_bombs;
};

} // namespace Bomberman

#endif // BOMB_MANAGER_H

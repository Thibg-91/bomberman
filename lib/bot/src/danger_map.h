#ifndef DANGER_MAP_H
#define DANGER_MAP_H

#pragma once

#include <functional>
#include <limits>
#include <vector>

#include "bomb.h"
#include "grid.h"
#include "position.h"

namespace Bomberman
{

// What the bombs on the grid are going to do, tick by tick: when each cell
// is going to be hit by an explosion, and from when a cell blocked by a bomb
// or a destructible block becomes walkable. It is the bot's "timeline" of
// the board, built with the game's own rules (Blast::compute) so it never
// disagrees with the BombManager.
//
// Ticks are counted from now: tick 0 is the tick being run (the one a
// controller decides in), so a bomb with one tick of fuse left explodes at
// tick 0. An explosion kills whoever stands on its cells *after* the
// commands of that tick: leaving a cell at tick t escapes its explosion at
// tick t.
class DangerMap
{
public:
    static constexpr int Never = -1;                              // no explosion
    static constexpr int Forever = std::numeric_limits<int>::max(); // never walkable

    // =========================
    // Constructors / Destructor
    // =========================
    DangerMap() = default;

    // =========================
    // Static Methods
    // =========================
    // Predicts the explosions of bombs on grid. Bombs explode one after the
    // other in their order (like BombManager::update): a block destroyed by
    // an explosion no longer stops the later ones. There is no chain
    // reaction in this game, so each bomb explodes when its own fuse ends.
    //
    // isNoticed models perception: a bomb it rejects still blocks its cell
    // (for good, as far as the map knows) but its explosion is not
    // predicted. Every bomb is noticed when it is empty.
    using BombFilter = std::function<bool(const Bomb&)>;
    [[nodiscard]] static DangerMap predict(const std::vector<Bomb>& bombs, const Grid& grid, int tickRate,
                                           const BombFilter& isNoticed = {});

    // Tick (counted from now, see above) at which bomb explodes.
    [[nodiscard]] static int explosionTick(const Bomb& bomb, int tickRate) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;

    // Whether an explosion hits position at tick / at a tick in
    // [first, last]. Always false outside the grid.
    [[nodiscard]] bool isHitAt(Position position, int tick) const;
    [[nodiscard]] bool isHitDuring(Position position, int first, int last) const;

    // Last tick at which position is hit, Never if it is safe.
    [[nodiscard]] int lastHit(Position position) const;
    // Whether an explosion is going to hit position at all.
    [[nodiscard]] bool isThreatened(Position position) const;
    // Every tick at which position is hit, in increasing order.
    [[nodiscard]] const std::vector<int>& hitsAt(Position position) const;

    // First tick from which a character can step onto position: 0 when it
    // is walkable now, the tick after the explosion that clears it for a
    // bomb or a block that is going to be destroyed, Forever otherwise
    // (walls, blocks no predicted explosion reaches, outside the grid).
    [[nodiscard]] int freeFrom(Position position) const;

    // From this tick on nothing changes any more: no more explosion, every
    // cell that opens has opened.
    [[nodiscard]] int getCalmTick() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] bool isInside(Position position) const noexcept;
    [[nodiscard]] std::size_t indexOf(Position position) const noexcept;

    // =========================
    // Members
    // =========================
    int m_width = 0;
    int m_height = 0;
    std::vector<std::vector<int>> m_hits; // per cell, sorted
    std::vector<int> m_freeFrom;          // per cell
    int m_calmTick = 0;
};

} // namespace Bomberman

#endif // DANGER_MAP_H

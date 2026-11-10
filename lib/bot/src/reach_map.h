#ifndef REACH_MAP_H
#define REACH_MAP_H

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "danger_map.h"
#include "direction.h"
#include "position.h"

namespace Bomberman
{

// Where a character can go without dying, and how fast: a path search
// through space *and time* over a DangerMap.
//
// A state is (cell, facing, tick at which the character can act again).
// From a state the character can, exactly like with the engine's commands:
//   - move forward: it is on the next cell at once and can act again
//     moveTicks later; it needs the cell to be free at that tick;
//   - turn towards another direction: turnTicks on the same cell;
//   - wait on the same cell.
// An action is only allowed if no explosion hits the cells the character
// stands on meanwhile. Waiting matters: the only way out may cross a cell
// that is about to explode, which is fine a tick after the explosion. So
// that a cell is not visited once per tick, the character only waits until
// a moment where something changes nearby: right after an explosion on a
// neighbour, just in time to step two cells further right after an
// explosion there, or when a neighbour opens.
//
// The result, for each cell (and each facing), is the earliest tick at
// which the character can stand there *for good* (no explosion hits it
// afterwards) and the first action to take now to get there that fast.
// Once every bomb has exploded (DangerMap::getCalmTick()) the board stops
// changing, so the search becomes a plain shortest path from there on.
class ReachMap
{
public:
    static constexpr int Unreachable = -1;

    // How much to explore: everything (to pick a goal among all reachable
    // cells), or only until the earliest safe cells are found (enough for
    // canSurvive(), nearestSafeCell() and the way there: much cheaper).
    enum class Scope
    {
        Everything,
        UntilSafe
    };

    // =========================
    // Constructors / Destructor
    // =========================
    ReachMap() = default;

    // =========================
    // Static Methods
    // =========================
    // Explores everything reachable from start, facing facing, the
    // character being able to act in delay ticks (0: right now; e.g.
    // placeBombTicks to evaluate what happens after dropping a bomb, the
    // character staying on start meanwhile).
    [[nodiscard]] static ReachMap explore(const DangerMap& danger, Position start, Direction facing, int delay,
                                          int moveTicks, int turnTicks, Scope scope = Scope::Everything);

    // =========================
    // Public Methods
    // =========================
    // Earliest tick at which the character can stand on position for good,
    // Unreachable if it cannot. With a facing: standing there facing that
    // direction (e.g. to drop a bomb on a given neighbour).
    [[nodiscard]] int safeArrival(Position position) const;
    [[nodiscard]] int safeArrival(Position position, Direction facing) const;
    [[nodiscard]] bool isSafelyReachable(Position position) const;

    // What to do now to get there in safeArrival() ticks: the direction to
    // give to CharacterCommand::move() (which turns or steps, see
    // Character), or nothing when waiting is the best move (or the
    // character is already there, or it cannot get there at all).
    [[nodiscard]] std::optional<Direction> firstStep(Position position) const;
    [[nodiscard]] std::optional<Direction> firstStep(Position position, Direction facing) const;

    // The cell the character can reach for good the soonest (the way out
    // of a blast), ties broken in reading order. Nothing when every way
    // ends in an explosion.
    [[nodiscard]] std::optional<Position> nearestSafeCell() const;
    [[nodiscard]] bool canSurvive() const;

    [[nodiscard]] Position getStart() const noexcept;
    // Number of states the search went through (tests, profiling).
    [[nodiscard]] int getExploredStates() const noexcept;

private:
    // First action leading to a state.
    static constexpr signed char NoAction = -1; // the start state itself
    static constexpr signed char WaitAction = 4; // 0..3: a Direction

    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] bool isInside(Position position) const noexcept;
    [[nodiscard]] std::size_t slot(Position position, Direction facing) const noexcept;
    // Facing with the earliest safe arrival on position (first of them on
    // a tie), nothing if unreachable.
    [[nodiscard]] std::optional<Direction> bestFacing(Position position) const;

    // =========================
    // Members
    // =========================
    int m_width = 0;
    int m_height = 0;
    Position m_start;
    std::vector<int> m_arrival;            // per (cell, facing)
    std::vector<signed char> m_firstStep;  // per (cell, facing)
    int m_exploredStates = 0;
};

} // namespace Bomberman

#endif // REACH_MAP_H

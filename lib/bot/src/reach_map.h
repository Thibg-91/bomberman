#ifndef REACH_MAP_H
#define REACH_MAP_H

#pragma once

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
// A state is (cell, tick at which the character can act again). From a
// state the character can, exactly like with the engine's commands:
//   - step to a neighbour (a move is immediate, whatever the facing): it is
//     on the next cell at once and can act again moveTicks later; it needs
//     the cell to be free at that tick;
//   - wait on the same cell.
// An action is only allowed if no explosion hits the cells the character
// stands on meanwhile. Waiting matters: the only way out may cross a cell
// that is about to explode, which is fine a tick after the explosion. So
// that a cell is not visited once per tick, the character only waits until
// a moment where something changes nearby: right after an explosion on a
// neighbour, just in time to step two cells further right after an
// explosion there, or when a neighbour opens.
//
// The start cell may hold a bomb (the one the character just dropped): it
// can always step off it, but not come back before it has exploded.
//
// The result, for each cell, is the earliest tick at which the character
// can stand there *for good* (no explosion hits it afterwards) and the
// first action to take now to get there that fast. Once every bomb has
// exploded (DangerMap::getCalmTick()) the board stops changing, so the
// search becomes a plain shortest path from there on.
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
    // Explores everything reachable from start, the character being able
    // to act in delay ticks (0: right now; e.g. placeBombTicks to evaluate
    // what happens after dropping a bomb, the character staying on start
    // meanwhile).
    [[nodiscard]] static ReachMap explore(const DangerMap& danger, Position start, int delay, int moveTicks,
                                          Scope scope = Scope::Everything);

    // =========================
    // Public Methods
    // =========================
    // Earliest tick at which the character can stand on position for good,
    // Unreachable if it cannot.
    [[nodiscard]] int safeArrival(Position position) const;
    [[nodiscard]] bool isSafelyReachable(Position position) const;

    // What to do now to get there in safeArrival() ticks: the direction to
    // give to CharacterCommand::move(), or nothing when waiting is the best
    // move (or the character is already there, or it cannot get there).
    [[nodiscard]] std::optional<Direction> firstStep(Position position) const;

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
    [[nodiscard]] std::size_t indexOf(Position position) const noexcept;

    // =========================
    // Members
    // =========================
    int m_width = 0;
    int m_height = 0;
    Position m_start;
    std::vector<int> m_arrival;           // per cell
    std::vector<signed char> m_firstStep; // per cell
    int m_exploredStates = 0;
};

} // namespace Bomberman

#endif // REACH_MAP_H

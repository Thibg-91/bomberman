#include "reach_map.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace Bomberman
{

namespace
{

constexpr int FacingCount = 4;
constexpr std::array<Direction, FacingCount> Facings = {Direction::Up, Direction::Down, Direction::Left,
                                                        Direction::Right};

int facingIndex(Direction direction) noexcept
{
    return static_cast<int>(direction);
}

// The ticks worth waiting on a cell until: right after an explosion on a
// neighbour (to step there once it has passed), early enough to be ready
// right after an explosion two cells away, and when a neighbour opens.
// They do not depend on when the character gets there, so they are
// computed once per cell (sorted, without duplicates).
std::vector<int> waitTargets(const DangerMap& danger, Position cell, int moveTicks)
{
    std::vector<int> targets;
    for (const Direction dir : Facings)
    {
        const Position neighbour = step(cell, dir);
        const int opens = danger.freeFrom(neighbour);
        if (opens != DangerMap::Forever && opens > 0)
            targets.push_back(opens);
        for (const int hit : danger.hitsAt(neighbour))
            targets.push_back(hit + 1);
        for (const Direction further : Facings)
        {
            const Position beyond = step(neighbour, further);
            if (beyond != cell)
                for (const int hit : danger.hitsAt(beyond))
                    targets.push_back(hit + 1 - moveTicks);
        }
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
}

struct State
{
    Position cell;
    Direction facing;
    signed char firstAction;
    int next; // next state of the same tick (index in the state pool), -1 for the last
};

} // namespace

// =========================
// Static Methods
// =========================
ReachMap ReachMap::explore(const DangerMap& danger, Position start, Direction facing, int delay, int moveTicks,
                           int turnTicks, Scope scope)
{
    ReachMap map;
    map.m_width = danger.getWidth();
    map.m_height = danger.getHeight();
    map.m_start = start;
    const std::size_t slots =
        static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height) * FacingCount;
    map.m_arrival.assign(slots, Unreachable);
    map.m_firstStep.assign(slots, NoAction);
    if (!map.isInside(start))
        return map;

    moveTicks = std::max(moveTicks, 1);
    turnTicks = std::max(turnTicks, 1);
    delay = std::max(delay, 0);
    // The character stays on its cell until it can act.
    if (delay > 0 && danger.isHitDuring(start, 0, delay - 1))
        return map;

    // Before calm, a state is (cell, facing, tick): the same cell can be
    // worth visiting again later, once an explosion has passed. From calm
    // on nothing changes any more: the earliest visit of (cell, facing) is
    // the only one that matters (Dijkstra).
    const int calm = danger.getCalmTick();
    std::vector<char> visitedBeforeCalm(slots * static_cast<std::size_t>(std::max(calm, 0)), 0);
    std::vector<int> earliestAfterCalm(slots, std::numeric_limits<int>::max());

    // Every action takes at least one tick: process the ticks in increasing
    // order, each with the states reached at that tick (first come, first
    // served, which keeps the result deterministic). The states of a tick
    // are chained in one pool (no allocation per tick).
    std::vector<State> pool;
    pool.reserve(slots);
    std::vector<std::vector<int>> waits(slots / FacingCount);
    std::vector<char> waitComputed(slots / FacingCount, 0);
    std::vector<int> first(static_cast<std::size_t>(std::max(calm, delay) + 1), -1);
    std::vector<int> last(first.size(), -1);
    auto push = [&](Position cell, Direction dir, int tick, signed char firstAction) {
        const std::size_t s = map.slot(cell, dir);
        if (tick < calm)
        {
            char& visited = visitedBeforeCalm[s * static_cast<std::size_t>(calm) + static_cast<std::size_t>(tick)];
            if (visited)
                return;
            visited = 1;
        }
        else
        {
            if (tick >= earliestAfterCalm[s])
                return;
            earliestAfterCalm[s] = tick;
        }
        const std::size_t t = static_cast<std::size_t>(tick);
        if (first.size() <= t)
        {
            first.resize(t + 1, -1);
            last.resize(t + 1, -1);
        }
        const int index = static_cast<int>(pool.size());
        pool.push_back(State{cell, dir, firstAction, -1});
        if (last[t] < 0)
            first[t] = index;
        else
            pool[static_cast<std::size_t>(last[t])].next = index;
        last[t] = index;
    };

    push(start, facing, delay, NoAction);
    bool foundSafe = false;
    for (std::size_t t = 0; t < first.size(); ++t)
    {
        // The whole tick is processed so that nearestSafeCell() breaks ties
        // the same way in both scopes.
        if (foundSafe && scope == Scope::UntilSafe)
            break;
        const int tick = static_cast<int>(t);
        for (int i = first[t]; i >= 0; i = pool[static_cast<std::size_t>(i)].next)
        {
            const State state = pool[static_cast<std::size_t>(i)]; // copy: push() may reallocate the pool
            const std::size_t s = map.slot(state.cell, state.facing);
            if (tick >= calm && earliestAfterCalm[s] != tick)
                continue; // superseded by an earlier visit
            ++map.m_exploredStates;

            // Standing here from now on is safe: the earliest such tick is
            // the answer for this (cell, facing).
            if (map.m_arrival[s] == Unreachable && tick > danger.lastHit(state.cell))
            {
                map.m_arrival[s] = tick;
                map.m_firstStep[s] = state.firstAction;
                foundSafe = true;
            }
            if (foundSafe && scope == Scope::UntilSafe)
                continue;

            auto firstAction = [&](signed char action) {
                return state.firstAction == NoAction ? action : state.firstAction;
            };

            // Step forward.
            const Position next = step(state.cell, state.facing);
            if (danger.freeFrom(next) <= tick && !danger.isHitDuring(next, tick, tick + moveTicks - 1))
                push(next, state.facing, tick + moveTicks,
                     firstAction(static_cast<signed char>(facingIndex(state.facing))));

            // Turn (allowed towards a blocked cell too).
            if (!danger.isHitDuring(state.cell, tick, tick + turnTicks - 1))
                for (const Direction dir : Facings)
                    if (dir != state.facing)
                        push(state.cell, dir, tick + turnTicks,
                             firstAction(static_cast<signed char>(facingIndex(dir))));

            // Wait, but only until something changes around (waiting one
            // tick at a time would visit every (cell, facing) once per
            // tick), and never past the next explosion on this cell: the
            // character must have left by then.
            if (tick < calm)
            {
                const std::size_t cellIndex = s / FacingCount;
                if (!waitComputed[cellIndex])
                {
                    waits[cellIndex] = waitTargets(danger, state.cell, moveTicks);
                    waitComputed[cellIndex] = 1;
                }
                const std::vector<int>& ownHits = danger.hitsAt(state.cell);
                const auto nextOwnHit = std::lower_bound(ownHits.begin(), ownHits.end(), tick);
                const int latest = std::min(nextOwnHit == ownHits.end() ? calm : *nextOwnHit, calm);
                const std::vector<int>& targets = waits[cellIndex];
                for (auto it = std::upper_bound(targets.begin(), targets.end(), tick);
                     it != targets.end() && *it <= latest; ++it)
                    push(state.cell, state.facing, *it, firstAction(WaitAction));
            }
        }
    }
    return map;
}

// =========================
// Public Methods
// =========================
int ReachMap::safeArrival(Position position) const
{
    const std::optional<Direction> facing = bestFacing(position);
    return facing ? safeArrival(position, *facing) : Unreachable;
}

int ReachMap::safeArrival(Position position, Direction facing) const
{
    return isInside(position) ? m_arrival[slot(position, facing)] : Unreachable;
}

bool ReachMap::isSafelyReachable(Position position) const
{
    return safeArrival(position) != Unreachable;
}

std::optional<Direction> ReachMap::firstStep(Position position) const
{
    const std::optional<Direction> facing = bestFacing(position);
    return facing ? firstStep(position, *facing) : std::nullopt;
}

std::optional<Direction> ReachMap::firstStep(Position position, Direction facing) const
{
    if (!isInside(position))
        return std::nullopt;
    const signed char action = m_firstStep[slot(position, facing)];
    if (action < 0 || action >= FacingCount)
        return std::nullopt;
    return Facings[static_cast<std::size_t>(action)];
}

std::optional<Position> ReachMap::nearestSafeCell() const
{
    std::optional<Position> nearest;
    int best = std::numeric_limits<int>::max();
    for (int y = 0; y < m_height; ++y)
        for (int x = 0; x < m_width; ++x)
        {
            const int arrival = safeArrival({x, y});
            if (arrival != Unreachable && arrival < best)
            {
                best = arrival;
                nearest = Position{x, y};
            }
        }
    return nearest;
}

bool ReachMap::canSurvive() const
{
    return nearestSafeCell().has_value();
}

Position ReachMap::getStart() const noexcept
{
    return m_start;
}

int ReachMap::getExploredStates() const noexcept
{
    return m_exploredStates;
}

// =========================
// Private Methods
// =========================
bool ReachMap::isInside(Position position) const noexcept
{
    return position.x >= 0 && position.y >= 0 && position.x < m_width && position.y < m_height;
}

std::size_t ReachMap::slot(Position position, Direction facing) const noexcept
{
    const std::size_t cell = static_cast<std::size_t>(position.y) * static_cast<std::size_t>(m_width)
                           + static_cast<std::size_t>(position.x);
    return cell * FacingCount + static_cast<std::size_t>(facingIndex(facing));
}

std::optional<Direction> ReachMap::bestFacing(Position position) const
{
    if (!isInside(position))
        return std::nullopt;
    std::optional<Direction> best;
    for (const Direction dir : Facings)
    {
        const int arrival = m_arrival[slot(position, dir)];
        if (arrival != Unreachable && (!best || arrival < m_arrival[slot(position, *best)]))
            best = dir;
    }
    return best;
}

} // namespace Bomberman

#include "reach_map.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

namespace Bomberman
{

namespace
{

constexpr std::array<Direction, 4> Directions = {Direction::Up, Direction::Down, Direction::Left, Direction::Right};

// The ticks worth waiting on a cell until: right after an explosion on a
// neighbour (to step there once it has passed), early enough to be ready
// right after an explosion two cells away, and when a neighbour opens.
// They do not depend on when the character gets there, so they are
// computed once per cell (sorted, without duplicates).
std::vector<int> waitTargets(const DangerMap& danger, Position cell, int moveTicks)
{
    std::vector<int> targets;
    for (const Direction dir : Directions)
    {
        const Position neighbour = step(cell, dir);
        const int opens = danger.freeFrom(neighbour);
        if (opens != DangerMap::Forever && opens > 0)
            targets.push_back(opens);
        for (const int hit : danger.hitsAt(neighbour))
            targets.push_back(hit + 1);
        for (const Direction further : Directions)
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
    signed char firstAction;
    int next; // next state of the same tick (index in the state pool), -1 for the last
};

} // namespace

// =========================
// Static Methods
// =========================
ReachMap ReachMap::explore(const DangerMap& danger, Position start, int delay, int moveTicks, Scope scope)
{
    ReachMap map;
    map.m_width = danger.getWidth();
    map.m_height = danger.getHeight();
    map.m_start = start;
    const std::size_t cells = static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height);
    map.m_arrival.assign(cells, Unreachable);
    map.m_firstStep.assign(cells, NoAction);
    if (!map.isInside(start))
        return map;

    moveTicks = std::max(moveTicks, 1);
    delay = std::max(delay, 0);
    // The character stays on its cell until it can act.
    if (delay > 0 && danger.isHitDuring(start, 0, delay - 1))
        return map;

    // Before calm, a state is (cell, tick): the same cell can be worth
    // visiting again later, once an explosion has passed. From calm on
    // nothing changes any more: the earliest visit of a cell is the only one
    // that matters (Dijkstra).
    const int calm = danger.getCalmTick();
    std::vector<char> visitedBeforeCalm(cells * static_cast<std::size_t>(std::max(calm, 0)), 0);
    std::vector<int> earliestAfterCalm(cells, std::numeric_limits<int>::max());

    // Every action takes at least one tick: process the ticks in increasing
    // order, each with the states reached at that tick (first come, first
    // served, which keeps the result deterministic). The states of a tick
    // are chained in one pool (no allocation per tick).
    std::vector<State> pool;
    pool.reserve(cells);
    std::vector<std::vector<int>> waits(cells);
    std::vector<char> waitComputed(cells, 0);
    std::vector<int> first(static_cast<std::size_t>(std::max(calm, delay) + 1), -1);
    std::vector<int> last(first.size(), -1);
    auto push = [&](Position cell, int tick, signed char firstAction) {
        const std::size_t c = map.indexOf(cell);
        if (tick < calm)
        {
            char& visited = visitedBeforeCalm[c * static_cast<std::size_t>(calm) + static_cast<std::size_t>(tick)];
            if (visited)
                return;
            visited = 1;
        }
        else
        {
            if (tick >= earliestAfterCalm[c])
                return;
            earliestAfterCalm[c] = tick;
        }
        const std::size_t t = static_cast<std::size_t>(tick);
        if (first.size() <= t)
        {
            first.resize(t + 1, -1);
            last.resize(t + 1, -1);
        }
        const int index = static_cast<int>(pool.size());
        pool.push_back(State{cell, firstAction, -1});
        if (last[t] < 0)
            first[t] = index;
        else
            pool[static_cast<std::size_t>(last[t])].next = index;
        last[t] = index;
    };

    push(start, delay, NoAction);
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
            const std::size_t c = map.indexOf(state.cell);
            if (tick >= calm && earliestAfterCalm[c] != tick)
                continue; // superseded by an earlier visit
            ++map.m_exploredStates;

            // Standing here from now on is safe: the earliest such tick is
            // the answer for this cell.
            if (map.m_arrival[c] == Unreachable && tick > danger.lastHit(state.cell))
            {
                map.m_arrival[c] = tick;
                map.m_firstStep[c] = state.firstAction;
                foundSafe = true;
            }
            if (foundSafe && scope == Scope::UntilSafe)
                continue;

            auto firstAction = [&](signed char action) {
                return state.firstAction == NoAction ? action : state.firstAction;
            };

            // Step to a neighbour.
            for (const Direction dir : Directions)
            {
                const Position next = step(state.cell, dir);
                if (danger.freeFrom(next) <= tick && !danger.isHitDuring(next, tick, tick + moveTicks - 1))
                    push(next, tick + moveTicks, firstAction(static_cast<signed char>(dir)));
            }

            // Wait, but only until something changes around (waiting one
            // tick at a time would visit every cell once per tick), and
            // never past the next explosion on this cell: the character must
            // have left by then.
            if (tick < calm)
            {
                if (!waitComputed[c])
                {
                    waits[c] = waitTargets(danger, state.cell, moveTicks);
                    waitComputed[c] = 1;
                }
                const std::vector<int>& ownHits = danger.hitsAt(state.cell);
                const auto nextOwnHit = std::lower_bound(ownHits.begin(), ownHits.end(), tick);
                const int latest = std::min(nextOwnHit == ownHits.end() ? calm : *nextOwnHit, calm);
                const std::vector<int>& targets = waits[c];
                for (auto it = std::upper_bound(targets.begin(), targets.end(), tick);
                     it != targets.end() && *it <= latest; ++it)
                    push(state.cell, *it, firstAction(WaitAction));
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
    return isInside(position) ? m_arrival[indexOf(position)] : Unreachable;
}

bool ReachMap::isSafelyReachable(Position position) const
{
    return safeArrival(position) != Unreachable;
}

std::optional<Direction> ReachMap::firstStep(Position position) const
{
    if (!isInside(position))
        return std::nullopt;
    const signed char action = m_firstStep[indexOf(position)];
    if (action < 0 || action >= static_cast<signed char>(Directions.size()))
        return std::nullopt;
    return static_cast<Direction>(action);
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

std::size_t ReachMap::indexOf(Position position) const noexcept
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(m_width)
         + static_cast<std::size_t>(position.x);
}

} // namespace Bomberman

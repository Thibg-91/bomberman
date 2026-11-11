#include "danger_map.h"

#include <algorithm>
#include <cmath>

#include "blast.h"

namespace Bomberman
{

namespace
{

const std::vector<int> NoHits;

} // namespace

// =========================
// Static Methods
// =========================
DangerMap DangerMap::predict(const std::vector<Bomb>& bombs, const Grid& grid, int tickRate,
                             const BombFilter& isNoticed)
{
    DangerMap map;
    map.m_width = grid.getWidth();
    map.m_height = grid.getHeight();
    const std::size_t cellCount = static_cast<std::size_t>(map.m_width) * static_cast<std::size_t>(map.m_height);
    map.m_hits.assign(cellCount, {});
    map.m_freeFrom.assign(cellCount, Forever);
    for (int y = 0; y < map.m_height; ++y)
        for (int x = 0; x < map.m_width; ++x)
            if (grid.isWalkable({x, y}))
                map.m_freeFrom[map.indexOf({x, y})] = 0;

    // Explosion order: by tick, then in the bombs' order (stable sort), as
    // BombManager::update walks its vector. An unnoticed bomb just blocks
    // its cell.
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < bombs.size(); ++i)
    {
        if (!isNoticed || isNoticed(bombs[i]))
            order.push_back(i);
        else if (map.isInside(bombs[i].getPosition()))
            map.m_freeFrom[map.indexOf(bombs[i].getPosition())] = Forever;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return explosionTick(bombs[a], tickRate) < explosionTick(bombs[b], tickRate);
    });

    // Played on a copy so that each blast sees the blocks destroyed by the
    // previous ones.
    Grid board = grid;
    for (const std::size_t i : order)
    {
        const Bomb& bomb = bombs[i];
        const int tick = explosionTick(bomb, tickRate);
        const Blast blast = Blast::compute(bomb.getPosition(), bomb.getBlastRadius(), board);
        for (const Position cell : blast.cells())
        {
            map.m_hits[map.indexOf(cell)].push_back(tick);
            // Only arm ends can be destructible blocks (Blast::compute).
            if (board.getTile(cell).destroy())
                map.m_freeFrom[map.indexOf(cell)] = tick + 1;
        }
        if (map.isInside(bomb.getPosition()))
            map.m_freeFrom[map.indexOf(bomb.getPosition())] = tick + 1; // the bomb is gone
        map.m_calmTick = std::max(map.m_calmTick, tick + 1);
    }

    for (std::vector<int>& hits : map.m_hits)
        std::sort(hits.begin(), hits.end());
    return map;
}

int DangerMap::explosionTick(const Bomb& bomb, int tickRate) noexcept
{
    // A fuse of n ticks burns once per tick, starting with this one.
    const long ticksLeft = std::lround(bomb.getTimeRemaining() * static_cast<float>(tickRate));
    return static_cast<int>(std::max(ticksLeft - 1, 0L));
}

// =========================
// Public Methods
// =========================
int DangerMap::getWidth() const noexcept
{
    return m_width;
}

int DangerMap::getHeight() const noexcept
{
    return m_height;
}

bool DangerMap::isHitAt(Position position, int tick) const
{
    return isHitDuring(position, tick, tick);
}

bool DangerMap::isHitDuring(Position position, int first, int last) const
{
    const std::vector<int>& hits = hitsAt(position);
    const auto it = std::lower_bound(hits.begin(), hits.end(), first);
    return it != hits.end() && *it <= last;
}

int DangerMap::lastHit(Position position) const
{
    const std::vector<int>& hits = hitsAt(position);
    return hits.empty() ? Never : hits.back();
}

bool DangerMap::isThreatened(Position position) const
{
    return !hitsAt(position).empty();
}

const std::vector<int>& DangerMap::hitsAt(Position position) const
{
    return isInside(position) ? m_hits[indexOf(position)] : NoHits;
}

int DangerMap::freeFrom(Position position) const
{
    return isInside(position) ? m_freeFrom[indexOf(position)] : Forever;
}

int DangerMap::getCalmTick() const noexcept
{
    return m_calmTick;
}

// =========================
// Private Methods
// =========================
bool DangerMap::isInside(Position position) const noexcept
{
    return position.x >= 0 && position.y >= 0 && position.x < m_width && position.y < m_height;
}

std::size_t DangerMap::indexOf(Position position) const noexcept
{
    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(m_width)
         + static_cast<std::size_t>(position.x);
}

} // namespace Bomberman

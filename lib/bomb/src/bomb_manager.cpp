#include "bomb_manager.h"

#include <algorithm>
#include <cstddef>

namespace Bomberman
{

// =========================
// Public Methods
// =========================
const std::vector<Bomb>& BombManager::getBombs() const noexcept
{
    return m_bombs;
}

bool BombManager::hasBombAt(Position position) const noexcept
{
    return std::any_of(m_bombs.begin(), m_bombs.end(),
                        [position](const Bomb& bomb) { return bomb.getPosition() == position; });
}

std::vector<Position> BombManager::getBombPositions() const
{
    std::vector<Position> positions;
    positions.reserve(m_bombs.size());
    for (const Bomb& bomb : m_bombs)
        positions.push_back(bomb.getPosition());
    return positions;
}

int BombManager::countActiveBombs(int ownerId) const noexcept
{
    return static_cast<int>(std::count_if(m_bombs.begin(), m_bombs.end(), [ownerId](const Bomb& bomb) {
        return bomb.getOwnerId() == ownerId && !bomb.hasExploded();
    }));
}

bool BombManager::placeBomb(Position position, const Grid& grid, int ownerId, int maxActiveBombs, int blastRadius)
{
    if (!grid.isWalkable(position) || hasBombAt(position) || countActiveBombs(ownerId) >= maxActiveBombs)
        return false;

    m_bombs.emplace_back(position, Bomb::DefaultFuseDuration, blastRadius, ownerId);
    return true;
}

std::optional<Position> BombManager::pushBombAt(Position position, Direction direction, const Grid& grid,
                                                int distance, const std::vector<Position>& obstacles)
{
    const auto it = std::find_if(m_bombs.begin(), m_bombs.end(),
                                  [position](const Bomb& bomb) { return bomb.getPosition() == position; });
    if (it == m_bombs.end())
        return std::nullopt;

    // Slide cell by cell, stopping before the first blocked one.
    Position target = position;
    for (int i = 0; i < distance; ++i)
    {
        const Position next = step(target, direction);
        if (!grid.isWalkable(next) || hasBombAt(next)
            || std::find(obstacles.begin(), obstacles.end(), next) != obstacles.end())
            break;
        target = next;
    }

    if (target == position)
        return std::nullopt;

    it->setPosition(target);
    return target;
}

std::vector<Position> BombManager::update(float deltaSeconds, Grid& grid, std::vector<Position>* destroyedBlocks,
                                          std::vector<Blast>* blasts)
{
    std::vector<Position> hitCells;

    // Bombs explode one after the other: a block destroyed by one no longer
    // stops the next one.
    for (Bomb& bomb : m_bombs)
    {
        if (bomb.hasExploded())
            continue;

        bomb.update(deltaSeconds);
        if (!bomb.hasExploded())
            continue;

        const Blast blast = explode(bomb.getPosition(), bomb.getBlastRadius(), grid, destroyedBlocks);
        const std::vector<Position> cells = blast.cells();
        hitCells.insert(hitCells.end(), cells.begin(), cells.end());
        if (blasts)
            blasts->push_back(blast);
    }

    m_bombs.erase(std::remove_if(m_bombs.begin(), m_bombs.end(),
                                  [](const Bomb& bomb) { return bomb.hasExploded(); }),
                  m_bombs.end());

    return hitCells;
}

// =========================
// Private Methods
// =========================
Blast BombManager::explode(Position center, int blastRadius, Grid& grid, std::vector<Position>* destroyedBlocks)
{
    const Blast blast = Blast::compute(center, blastRadius, grid);

    // Only the last cell of an arm can be a destructible block.
    for (std::size_t arm = 0; arm < Blast::Directions.size(); ++arm)
    {
        if (blast.reach[arm] == 0)
            continue;
        Position cell = center;
        for (int i = 0; i < blast.reach[arm]; ++i)
            cell = step(cell, Blast::Directions[arm]);

        Tile& tile = grid.getTile(cell);
        if (tile.destroy() && destroyedBlocks)
            destroyedBlocks->push_back(cell);
    }
    return blast;
}

} // namespace Bomberman

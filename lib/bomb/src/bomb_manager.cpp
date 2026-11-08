#include "bomb_manager.h"

#include <algorithm>
#include <array>

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

bool BombManager::placeBomb(Position position, const Grid& grid)
{
    if (!grid.isWalkable(position) || hasBombAt(position))
        return false;

    m_bombs.emplace_back(position);
    return true;
}

bool BombManager::pushBombAt(Position position, Direction direction, const Grid& grid, int distance) noexcept
{
    const auto it = std::find_if(m_bombs.begin(), m_bombs.end(),
                                  [position](const Bomb& bomb) { return bomb.getPosition() == position; });
    if (it == m_bombs.end())
        return false;

    Position target = position;
    for (int i = 0; i < distance; ++i)
        target = step(target, direction);

    if (!grid.isWalkable(target) || hasBombAt(target))
        return false;

    it->setPosition(target);
    return true;
}

std::vector<Position> BombManager::update(float deltaSeconds, Grid& grid)
{
    std::vector<Position> hitCells;

    for (Bomb& bomb : m_bombs)
    {
        if (bomb.hasExploded())
            continue;

        bomb.update(deltaSeconds);
        if (bomb.hasExploded())
            explode(bomb.getPosition(), bomb.getBlastRadius(), grid, hitCells);
    }

    m_bombs.erase(std::remove_if(m_bombs.begin(), m_bombs.end(),
                                  [](const Bomb& bomb) { return bomb.hasExploded(); }),
                  m_bombs.end());

    return hitCells;
}

// =========================
// Private Methods
// =========================
void BombManager::explode(Position center, int blastRadius, Grid& grid, std::vector<Position>& hitCells) const
{
    hitCells.push_back(center);

    constexpr std::array<Direction, 4> arms = {Direction::Up, Direction::Down, Direction::Left, Direction::Right};
    for (Direction direction : arms)
    {
        Position cell = center;
        for (int i = 0; i < blastRadius; ++i)
        {
            cell = step(cell, direction);
            if (!grid.isInside(cell))
                break;

            Tile& tile = grid.getTile(cell);
            if (tile.blocksExplosion())
            {
                if (tile.isDestructible())
                {
                    tile.destroy();
                    hitCells.push_back(cell);
                }
                break;
            }

            hitCells.push_back(cell);
        }
    }
}

} // namespace Bomberman

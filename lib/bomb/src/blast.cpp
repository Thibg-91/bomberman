#include "blast.h"

#include <cstddef>

namespace Bomberman
{

// =========================
// Public Methods
// =========================
Blast Blast::compute(Position center, int blastRadius, const Grid& grid)
{
    Blast blast;
    blast.center = center;
    for (std::size_t arm = 0; arm < Directions.size(); ++arm)
    {
        Position cell = center;
        for (int i = 0; i < blastRadius; ++i)
        {
            cell = step(cell, Directions[arm]);
            if (!grid.isInside(cell))
                break;

            const Tile& tile = grid.getTile(cell);
            if (tile.blocksExplosion() && !tile.isDestructible())
                break; // walls stop the arm before them
            ++blast.reach[arm];
            if (tile.blocksExplosion())
                break; // a destructible block is hit, and stops the arm
        }
    }
    return blast;
}

int Blast::reachTowards(Direction direction) const noexcept
{
    for (std::size_t arm = 0; arm < Directions.size(); ++arm)
        if (Directions[arm] == direction)
            return reach[arm];
    return 0;
}

std::vector<Position> Blast::cells() const
{
    std::vector<Position> result = {center};
    for (std::size_t arm = 0; arm < Directions.size(); ++arm)
    {
        Position cell = center;
        for (int i = 0; i < reach[arm]; ++i)
        {
            cell = step(cell, Directions[arm]);
            result.push_back(cell);
        }
    }
    return result;
}

} // namespace Bomberman

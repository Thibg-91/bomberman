#include "grid.h"

#include <stdexcept>
#include <string>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Grid::Grid(int width, int height, TileType fill)
    : m_width(width),
      m_height(height)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Grid dimensions must be strictly positive");

    m_tiles.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), Tile(fill));
}

// =========================
// Public Methods
// =========================
int Grid::getWidth() const noexcept
{
    return m_width;
}

int Grid::getHeight() const noexcept
{
    return m_height;
}

bool Grid::isInside(Position position) const noexcept
{
    return position.x >= 0 && position.x < m_width
        && position.y >= 0 && position.y < m_height;
}

const Tile& Grid::getTile(Position position) const
{
    return m_tiles[indexOf(position)];
}

Tile& Grid::getTile(Position position)
{
    return m_tiles[indexOf(position)];
}

void Grid::setTile(Position position, Tile tile)
{
    m_tiles[indexOf(position)] = tile;
}

bool Grid::isWalkable(Position position) const noexcept
{
    return isInside(position) && getTile(position).isWalkable();
}

const std::vector<Position>& Grid::getSpawnPoints() const noexcept
{
    return m_spawnPoints;
}

void Grid::addSpawnPoint(Position position)
{
    if (!isWalkable(position))
        throw std::invalid_argument("Spawn point must be on a walkable tile");

    m_spawnPoints.push_back(position);
}

// =========================
// Private Methods
// =========================
std::size_t Grid::indexOf(Position position) const
{
    if (!isInside(position))
        throw std::out_of_range("Position (" + std::to_string(position.x) + ", "
                                + std::to_string(position.y) + ") is outside the grid");

    return static_cast<std::size_t>(position.y) * static_cast<std::size_t>(m_width)
         + static_cast<std::size_t>(position.x);
}

} // namespace Bomberman

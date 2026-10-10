#include "tile.h"

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Tile::Tile(TileType type) noexcept
    : m_type(type)
{
}

// =========================
// Public Methods
// =========================
TileType Tile::getType() const noexcept
{
    return m_type;
}

bool Tile::isWalkable() const noexcept
{
    return m_type == TileType::Empty;
}

bool Tile::isDestructible() const noexcept
{
    return m_type == TileType::DestructibleBlock;
}

bool Tile::blocksExplosion() const noexcept
{
    return m_type != TileType::Empty;
}

bool Tile::destroy() noexcept
{
    if (!isDestructible())
        return false;

    m_type = TileType::Empty;
    return true;
}

} // namespace Bomberman

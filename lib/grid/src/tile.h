#ifndef TILE_H
#define TILE_H

#pragma once

namespace Bomberman
{

// Static content of a grid cell. Dynamic objects (players, bombs, bonuses)
// are not tiles: they will be entities living on top of the grid.
enum class TileType
{
    Empty,
    Wall,             // indestructible
    DestructibleBlock // destroyed by explosions, may hide a bonus later
};

class Tile
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    explicit Tile(TileType type = TileType::Empty) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] TileType getType() const noexcept;

    [[nodiscard]] bool isWalkable() const noexcept;
    [[nodiscard]] bool isDestructible() const noexcept;
    [[nodiscard]] bool blocksExplosion() const noexcept;

    // Turns a destructible block into an empty tile.
    // Returns true if the tile has actually been destroyed.
    bool destroy() noexcept;

    friend bool operator==(const Tile& lhs, const Tile& rhs) noexcept
    {
        return lhs.m_type == rhs.m_type;
    }

private:
    // =========================
    // Members
    // =========================
    TileType m_type;
};

} // namespace Bomberman

#endif // TILE_H

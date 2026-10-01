#ifndef GRID_H
#define GRID_H

#pragma once

#include <vector>

#include "position.h"
#include "tile.h"

namespace Bomberman
{

// Rectangular board made of tiles, stored row by row.
// The grid only knows about static content (walls, blocks) and where players
// start; it is purely logical and has no dependency on the rendering library.
class Grid
{
public:
    // Conventional Bomberman board: 13x11 playable cells surrounded by walls.
    static constexpr int ClassicWidth = 15;
    static constexpr int ClassicHeight = 13;

    // =========================
    // Constructors / Destructor
    // =========================
    Grid() = default;
    Grid(int width, int height, TileType fill = TileType::Empty);

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;

    [[nodiscard]] bool isInside(Position position) const noexcept;

    // Throw std::out_of_range when position is outside the grid.
    [[nodiscard]] const Tile& getTile(Position position) const;
    [[nodiscard]] Tile& getTile(Position position);
    void setTile(Position position, Tile tile);

    // False for any position outside the grid.
    [[nodiscard]] bool isWalkable(Position position) const noexcept;

    [[nodiscard]] const std::vector<Position>& getSpawnPoints() const noexcept;
    void addSpawnPoint(Position position);

private:
    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] std::size_t indexOf(Position position) const;

    // =========================
    // Members
    // =========================
    int m_width = 0;
    int m_height = 0;
    std::vector<Tile> m_tiles;
    std::vector<Position> m_spawnPoints;
};

} // namespace Bomberman

#endif // GRID_H

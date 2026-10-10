#include <stdexcept>

#include <gtest/gtest.h>

#include "grid.h"

using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

TEST(GridTest, IsFilledWithGivenTileType)
{
    const Grid grid(4, 3, TileType::Wall);

    EXPECT_EQ(grid.getWidth(), 4);
    EXPECT_EQ(grid.getHeight(), 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 4; ++x)
            EXPECT_EQ(grid.getTile({x, y}).getType(), TileType::Wall);
}

TEST(GridTest, RejectsInvalidDimensions)
{
    EXPECT_THROW(Grid(0, 5), std::invalid_argument);
    EXPECT_THROW(Grid(5, -1), std::invalid_argument);
}

TEST(GridTest, IsInsideChecksBounds)
{
    const Grid grid(Grid::ClassicWidth, Grid::ClassicHeight);

    EXPECT_TRUE(grid.isInside({0, 0}));
    EXPECT_TRUE(grid.isInside({14, 12}));
    EXPECT_FALSE(grid.isInside({15, 0}));
    EXPECT_FALSE(grid.isInside({0, 13}));
    EXPECT_FALSE(grid.isInside({-1, 0}));
}

TEST(GridTest, SetTileOnlyChangesTargetedCell)
{
    Grid grid(5, 5);

    grid.setTile({2, 3}, Tile(TileType::DestructibleBlock));

    EXPECT_EQ(grid.getTile({2, 3}).getType(), TileType::DestructibleBlock);
    EXPECT_EQ(grid.getTile({3, 2}).getType(), TileType::Empty);
}

TEST(GridTest, AccessOutsideGridThrows)
{
    Grid grid(5, 5);

    EXPECT_THROW((void)grid.getTile({5, 0}), std::out_of_range);
    EXPECT_THROW(grid.setTile({0, -1}, Tile()), std::out_of_range);
}

TEST(GridTest, OutsideIsNotWalkable)
{
    const Grid grid(3, 3);

    EXPECT_TRUE(grid.isWalkable({1, 1}));
    EXPECT_FALSE(grid.isWalkable({3, 1}));
}

TEST(GridTest, SpawnPointMustBeWalkable)
{
    Grid grid(3, 3);
    grid.setTile({0, 0}, Tile(TileType::Wall));

    grid.addSpawnPoint({1, 1});

    ASSERT_EQ(grid.getSpawnPoints().size(), 1u);
    EXPECT_EQ(grid.getSpawnPoints().front(), (Position{1, 1}));
    EXPECT_THROW(grid.addSpawnPoint({0, 0}), std::invalid_argument);
    EXPECT_THROW(grid.addSpawnPoint({4, 4}), std::invalid_argument);
}

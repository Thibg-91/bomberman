#include <algorithm>

#include <gtest/gtest.h>

#include "bomb_manager.h"

using Bomberman::Bomb;
using Bomberman::BombManager;
using Bomberman::Direction;
using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

// Walkable 7x7 board; tests add obstacles.
Grid openGrid()
{
    return Grid(7, 7);
}

bool contains(const std::vector<Position>& cells, Position position)
{
    return std::find(cells.begin(), cells.end(), position) != cells.end();
}

} // namespace

TEST(BombManagerTest, PlaceBombSucceedsOnAWalkableEmptyCell)
{
    const Grid grid = openGrid();
    BombManager manager;

    EXPECT_TRUE(manager.placeBomb({3, 3}, grid));
    ASSERT_EQ(manager.getBombs().size(), 1u);
    EXPECT_EQ(manager.getBombs().front().getPosition(), (Position{3, 3}));
}

TEST(BombManagerTest, PlaceBombFailsOnAnOccupiedCell)
{
    Grid grid = openGrid();
    grid.setTile({3, 3}, Tile(TileType::Wall));
    BombManager manager;

    EXPECT_FALSE(manager.placeBomb({3, 3}, grid));
    EXPECT_TRUE(manager.getBombs().empty());
}

TEST(BombManagerTest, PlaceBombFailsWhereABombAlreadyIs)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    EXPECT_FALSE(manager.placeBomb({3, 3}, grid));
    EXPECT_EQ(manager.getBombs().size(), 1u);
}

TEST(BombManagerTest, PushMovesTheBombByTheGivenDistance)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    EXPECT_TRUE(manager.pushBombAt({3, 3}, Direction::Right, grid, 2));

    EXPECT_FALSE(manager.hasBombAt({3, 3}));
    EXPECT_TRUE(manager.hasBombAt({5, 3}));
}

TEST(BombManagerTest, PushDefaultsToOneCell)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    EXPECT_TRUE(manager.pushBombAt({3, 3}, Direction::Up, grid));

    EXPECT_TRUE(manager.hasBombAt({3, 2}));
}

TEST(BombManagerTest, PushFailsWhenTheDestinationIsNotWalkable)
{
    Grid grid = openGrid();
    grid.setTile({4, 3}, Tile(TileType::Wall));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    EXPECT_FALSE(manager.pushBombAt({3, 3}, Direction::Right, grid));
    EXPECT_TRUE(manager.hasBombAt({3, 3}));
}

TEST(BombManagerTest, PushFailsWhenAnotherBombIsAlreadyThere)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));
    ASSERT_TRUE(manager.placeBomb({4, 3}, grid));

    EXPECT_FALSE(manager.pushBombAt({3, 3}, Direction::Right, grid));
}

TEST(BombManagerTest, PushFailsWhenThereIsNoBombAtThePosition)
{
    const Grid grid = openGrid();
    BombManager manager;

    EXPECT_FALSE(manager.pushBombAt({3, 3}, Direction::Right, grid));
}

TEST(BombManagerTest, UpdateDoesNothingBeforeTheFuseRunsOut)
{
    Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    const std::vector<Position> hitCells = manager.update(1.f, grid);

    EXPECT_TRUE(hitCells.empty());
    EXPECT_FALSE(manager.getBombs().empty());
}

TEST(BombManagerTest, ExplosionDestroysBlocksInACrossAndIsRemovedAfterwards)
{
    Grid grid = openGrid();
    grid.setTile({4, 3}, Tile(TileType::DestructibleBlock));
    grid.setTile({3, 2}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid);

    EXPECT_TRUE(manager.getBombs().empty());
    EXPECT_TRUE(contains(hitCells, Position{3, 3})); // center
    EXPECT_TRUE(contains(hitCells, Position{4, 3}));
    EXPECT_TRUE(contains(hitCells, Position{3, 2}));
    EXPECT_EQ(grid.getTile({4, 3}).getType(), TileType::Empty);
    EXPECT_EQ(grid.getTile({3, 2}).getType(), TileType::Empty);
}

TEST(BombManagerTest, ExplosionStopsAtAWallWithoutDestroyingItOrGoingFurther)
{
    Grid grid = openGrid();
    grid.setTile({4, 3}, Tile(TileType::Wall));
    grid.setTile({5, 3}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid);

    EXPECT_EQ(grid.getTile({4, 3}).getType(), TileType::Wall);
    EXPECT_EQ(grid.getTile({5, 3}).getType(), TileType::DestructibleBlock);
    EXPECT_FALSE(contains(hitCells, Position{4, 3}));
    EXPECT_FALSE(contains(hitCells, Position{5, 3}));
}

TEST(BombManagerTest, ExplosionStopsAfterDestroyingTheFirstBlockOnAnArm)
{
    Grid grid = openGrid();
    grid.setTile({3, 4}, Tile(TileType::DestructibleBlock));
    grid.setTile({3, 5}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid);

    EXPECT_EQ(grid.getTile({3, 4}).getType(), TileType::Empty);
    EXPECT_EQ(grid.getTile({3, 5}).getType(), TileType::DestructibleBlock);
    EXPECT_FALSE(contains(hitCells, Position{3, 5}));
}

TEST(BombManagerTest, DefaultBlastRadiusDoesNotReachTwoCellsAway)
{
    Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid);

    EXPECT_TRUE(contains(hitCells, Position{4, 3}));
    EXPECT_FALSE(contains(hitCells, Position{5, 3}));
}

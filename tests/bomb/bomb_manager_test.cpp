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

TEST(BombManagerTest, ACharacterHasOnlyOneActiveBombByDefault)
{
    Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid, 0));
    EXPECT_EQ(manager.countActiveBombs(0), 1);

    EXPECT_FALSE(manager.placeBomb({1, 1}, grid, 0));
    EXPECT_EQ(manager.getBombs().size(), 1u);

    // Once it has exploded, the character can drop another one.
    (void)manager.update(Bomb::DefaultFuseDuration, grid);
    EXPECT_EQ(manager.countActiveBombs(0), 0);
    EXPECT_TRUE(manager.placeBomb({1, 1}, grid, 0));
}

TEST(BombManagerTest, TheLimitIsPerCharacter)
{
    const Grid grid = openGrid();
    BombManager manager;

    EXPECT_TRUE(manager.placeBomb({3, 3}, grid, 0));
    EXPECT_TRUE(manager.placeBomb({1, 1}, grid, 1));
    EXPECT_FALSE(manager.placeBomb({5, 5}, grid, 1));
    EXPECT_EQ(manager.countActiveBombs(0), 1);
    EXPECT_EQ(manager.countActiveBombs(1), 1);
}

TEST(BombManagerTest, TheLimitCanBeRaised)
{
    const Grid grid = openGrid();
    BombManager manager;

    EXPECT_TRUE(manager.placeBomb({3, 3}, grid, 0, 2));
    EXPECT_TRUE(manager.placeBomb({1, 1}, grid, 0, 2));
    EXPECT_FALSE(manager.placeBomb({5, 5}, grid, 0, 2));
}

TEST(BombManagerTest, GetBombPositionsFollowsPlacedPushedAndExplodedBombs)
{
    Grid grid = openGrid();
    BombManager manager;
    EXPECT_TRUE(manager.getBombPositions().empty());

    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));
    ASSERT_TRUE(manager.pushBombAt({3, 3}, Direction::Right, grid));
    EXPECT_EQ(manager.getBombPositions(), (std::vector<Position>{{4, 3}}));

    (void)manager.update(Bomb::DefaultFuseDuration, grid);
    EXPECT_TRUE(manager.getBombPositions().empty());
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
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid, 0));
    ASSERT_TRUE(manager.placeBomb({4, 3}, grid, 1));

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

// =========================
// Upgrades: blast radius and push strength
// =========================
TEST(BombManagerTest, PlaceBombUsesTheGivenBlastRadius)
{
    const Grid grid = openGrid();
    BombManager manager;

    ASSERT_TRUE(manager.placeBomb({3, 3}, grid, 0, 1, 3));

    EXPECT_EQ(manager.getBombs().front().getBlastRadius(), 3);
}

TEST(BombManagerTest, ABiggerRadiusReachesFurther)
{
    Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid, 0, 1, 3));

    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid);

    EXPECT_TRUE(contains(hitCells, {0, 3}));
    EXPECT_TRUE(contains(hitCells, {6, 3}));
    EXPECT_TRUE(contains(hitCells, {3, 0}));
    EXPECT_TRUE(contains(hitCells, {3, 6}));
    EXPECT_EQ(hitCells.size(), 13u); // center + 4 arms of 3
}

TEST(BombManagerTest, UpdateReportsTheDestroyedBlocks)
{
    Grid grid = openGrid();
    grid.setTile({4, 3}, Tile(TileType::DestructibleBlock));
    grid.setTile({3, 1}, Tile(TileType::DestructibleBlock)); // out of reach (radius 1)
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 3}, grid));

    std::vector<Position> destroyedBlocks;
    (void)manager.update(Bomb::DefaultFuseDuration, grid, &destroyedBlocks);

    ASSERT_EQ(destroyedBlocks.size(), 1u);
    EXPECT_EQ(destroyedBlocks.front(), (Position{4, 3}));
    EXPECT_FALSE(grid.isWalkable({3, 1}));
}

TEST(BombManagerTest, PushReturnsWhereTheBombLanded)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({1, 3}, grid));

    EXPECT_EQ(manager.pushBombAt({1, 3}, Direction::Right, grid, 3), (Position{4, 3}));
    EXPECT_TRUE(manager.hasBombAt({4, 3}));
}

TEST(BombManagerTest, AStrongPushStopsBeforeTheFirstObstacle)
{
    Grid grid = openGrid();
    grid.setTile({5, 3}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({1, 3}, grid));

    // 4 cells asked, but the block on (5, 3) stops the slide on (4, 3).
    EXPECT_EQ(manager.pushBombAt({1, 3}, Direction::Right, grid, 4), (Position{4, 3}));
}

TEST(BombManagerTest, AStrongPushStopsBeforeAnotherBomb)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({1, 3}, grid, 0));
    ASSERT_TRUE(manager.placeBomb({4, 3}, grid, 1));

    EXPECT_EQ(manager.pushBombAt({1, 3}, Direction::Right, grid, 5), (Position{3, 3}));
}

TEST(BombManagerTest, AStrongPushStopsBeforeTheGivenObstacles)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({1, 3}, grid));

    // e.g. a character standing on (3, 3)
    EXPECT_EQ(manager.pushBombAt({1, 3}, Direction::Right, grid, 5, {{3, 3}}), (Position{2, 3}));
}

TEST(BombManagerTest, APushThatCannotMoveTheBombFails)
{
    const Grid grid = openGrid();
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({1, 3}, grid));

    EXPECT_FALSE(manager.pushBombAt({1, 3}, Direction::Right, grid, 5, {{2, 3}}).has_value());
    EXPECT_TRUE(manager.hasBombAt({1, 3}));
}

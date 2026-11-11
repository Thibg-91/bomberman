#include <vector>

#include <gtest/gtest.h>

#include "blast.h"
#include "bomb_manager.h"

using Bomberman::Blast;
using Bomberman::Bomb;
using Bomberman::BombManager;
using Bomberman::Direction;
using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

TEST(BlastTest, AnOpenCrossReachesTheFullRadius)
{
    const Grid grid(9, 9);

    const Blast blast = Blast::compute({4, 4}, 3, grid);

    EXPECT_EQ(blast.center, (Position{4, 4}));
    for (const Direction direction : Blast::Directions)
        EXPECT_EQ(blast.reachTowards(direction), 3);
}

TEST(BlastTest, ArmsStopBeforeWallsAndTheGridEdge)
{
    Grid grid(9, 9);
    grid.setTile({4, 2}, Tile(TileType::Wall));

    const Blast blast = Blast::compute({4, 4}, 6, grid);

    EXPECT_EQ(blast.reachTowards(Direction::Up), 1);    // wall on (4, 2)
    EXPECT_EQ(blast.reachTowards(Direction::Down), 4);  // edge after (4, 8)
    EXPECT_EQ(blast.reachTowards(Direction::Left), 4);  // edge after (0, 4)
    EXPECT_EQ(blast.reachTowards(Direction::Right), 4); // edge after (8, 4)
}

TEST(BlastTest, ADestructibleBlockIsHitAndStopsTheArm)
{
    Grid grid(9, 9);
    grid.setTile({6, 4}, Tile(TileType::DestructibleBlock));
    grid.setTile({7, 4}, Tile(TileType::DestructibleBlock));

    const Blast blast = Blast::compute({4, 4}, 4, grid);

    EXPECT_EQ(blast.reachTowards(Direction::Right), 2);
    EXPECT_FALSE(grid.isWalkable({6, 4})); // computing never destroys anything
}

TEST(BlastTest, ARadiusOfZeroOnlyHitsTheCenter)
{
    const Blast blast = Blast::compute({2, 2}, 0, Grid(5, 5));

    EXPECT_EQ(blast.cells(), (std::vector<Position>{{2, 2}}));
}

TEST(BlastTest, CellsListTheCenterThenEachArmOutwards)
{
    Blast blast;
    blast.center = {3, 3};
    blast.reach = {1, 0, 2, 1}; // up, down, left, right

    EXPECT_EQ(blast.cells(), (std::vector<Position>{{3, 3}, {3, 2}, {2, 3}, {1, 3}, {4, 3}}));
}

TEST(BlastTest, BombManagerReportsTheBlastsItApplies)
{
    Grid grid(9, 9);
    grid.setTile({4, 6}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({4, 4}, grid, 0, 1, 3));
    const Blast expected = Blast::compute({4, 4}, 3, grid);

    std::vector<Blast> blasts;
    const std::vector<Position> hitCells = manager.update(Bomb::DefaultFuseDuration, grid, nullptr, &blasts);

    ASSERT_EQ(blasts.size(), 1u);
    EXPECT_EQ(blasts.front(), expected);
    EXPECT_EQ(blasts.front().reachTowards(Direction::Down), 2);
    EXPECT_EQ(hitCells, expected.cells());
}

TEST(BlastTest, BombsExplodingTogetherAreAppliedOneAfterTheOther)
{
    // The first bomb destroys the block, so the second one's blast goes
    // through its cell.
    Grid grid(9, 3);
    grid.setTile({4, 1}, Tile(TileType::DestructibleBlock));
    BombManager manager;
    ASSERT_TRUE(manager.placeBomb({3, 1}, grid, 0, 1, 1));
    ASSERT_TRUE(manager.placeBomb({6, 1}, grid, 1, 1, 3));

    std::vector<Blast> blasts;
    (void)manager.update(Bomb::DefaultFuseDuration, grid, nullptr, &blasts);

    ASSERT_EQ(blasts.size(), 2u);
    EXPECT_EQ(blasts[0].reachTowards(Direction::Right), 1);
    EXPECT_EQ(blasts[1].reachTowards(Direction::Left), 3);
}

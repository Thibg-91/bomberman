#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "bomb.h"
#include "bomb_manager.h"
#include "danger_map.h"
#include "grid.h"

using Bomberman::Bomb;
using Bomberman::BombManager;
using Bomberman::DangerMap;
using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

constexpr int TickRate = 60;
constexpr float Tick = 1.f / static_cast<float>(TickRate);
constexpr int FreshBombTick = 119; // a 2 s fuse burns on ticks 0..119

// A bomb that has already been ticking for ticks ticks.
Bomb ticking(Position position, int ticks, int blastRadius = 1)
{
    Bomb bomb(position, Bomb::DefaultFuseDuration, blastRadius);
    for (int i = 0; i < ticks; ++i)
        bomb.update(Tick);
    return bomb;
}

} // namespace

TEST(DangerMapTest, NothingIsThreatenedWithoutBombs)
{
    Grid grid(5, 5);
    grid.setTile({2, 2}, Tile(TileType::Wall));
    grid.setTile({3, 3}, Tile(TileType::DestructibleBlock));

    const DangerMap danger = DangerMap::predict({}, grid, TickRate);

    EXPECT_EQ(danger.getWidth(), 5);
    EXPECT_EQ(danger.getHeight(), 5);
    EXPECT_FALSE(danger.isThreatened({1, 1}));
    EXPECT_EQ(danger.lastHit({1, 1}), DangerMap::Never);
    EXPECT_EQ(danger.getCalmTick(), 0);
    EXPECT_EQ(danger.freeFrom({1, 1}), 0);
    EXPECT_EQ(danger.freeFrom({2, 2}), DangerMap::Forever); // wall
    EXPECT_EQ(danger.freeFrom({3, 3}), DangerMap::Forever); // block nothing destroys
    EXPECT_EQ(danger.freeFrom({-1, 0}), DangerMap::Forever);
    EXPECT_FALSE(danger.isHitAt({7, 7}, 0));
}

TEST(DangerMapTest, ExplosionTickCountsTheTickBeingRunAsTickZero)
{
    EXPECT_EQ(DangerMap::explosionTick(Bomb({1, 1}), TickRate), FreshBombTick);
    EXPECT_EQ(DangerMap::explosionTick(ticking({1, 1}, 60), TickRate), 59);
    EXPECT_EQ(DangerMap::explosionTick(ticking({1, 1}, 119), TickRate), 0);
}

TEST(DangerMapTest, AFreshBombHitsItsCrossAtTheEndOfItsFuse)
{
    const Grid grid(7, 7);
    const DangerMap danger = DangerMap::predict({Bomb({3, 3})}, grid, TickRate);

    for (const Position cell : {Position{3, 3}, Position{3, 2}, Position{3, 4}, Position{2, 3}, Position{4, 3}})
    {
        EXPECT_TRUE(danger.isHitAt(cell, FreshBombTick));
        EXPECT_FALSE(danger.isHitDuring(cell, 0, FreshBombTick - 1));
        EXPECT_EQ(danger.lastHit(cell), FreshBombTick);
        EXPECT_EQ(danger.hitsAt(cell), (std::vector<int>{FreshBombTick}));
    }
    EXPECT_FALSE(danger.isThreatened({1, 3})); // radius 1
    EXPECT_FALSE(danger.isThreatened({2, 2})); // a cross, not a square
    EXPECT_EQ(danger.freeFrom({3, 3}), FreshBombTick + 1); // the bomb's cell opens once it has exploded
    EXPECT_EQ(danger.getCalmTick(), FreshBombTick + 1);
}

TEST(DangerMapTest, WallsStopTheBlastAndHitBlocksOpenAfterwards)
{
    Grid grid(7, 7);
    grid.setTile({3, 1}, Tile(TileType::Wall));
    grid.setTile({5, 3}, Tile(TileType::DestructibleBlock));

    const DangerMap danger = DangerMap::predict({Bomb({3, 3}, Bomb::DefaultFuseDuration, 3)}, grid, TickRate);

    EXPECT_TRUE(danger.isThreatened({3, 2}));
    EXPECT_FALSE(danger.isThreatened({3, 1})); // the wall stops the arm
    EXPECT_FALSE(danger.isThreatened({3, 0}));
    EXPECT_TRUE(danger.isThreatened({5, 3})); // the block is hit...
    EXPECT_FALSE(danger.isThreatened({6, 3})); // ...and stops the arm
    EXPECT_EQ(danger.freeFrom({5, 3}), FreshBombTick + 1);
    EXPECT_EQ(danger.freeFrom({3, 1}), DangerMap::Forever);
}

TEST(DangerMapTest, ABlockDestroyedByAnEarlierBombNoLongerStopsALaterOne)
{
    Grid grid(9, 3);
    grid.setTile({3, 1}, Tile(TileType::DestructibleBlock));
    const std::vector<Bomb> bombs = {ticking({2, 1}, 60), Bomb({5, 1}, Bomb::DefaultFuseDuration, 3)};

    const DangerMap danger = DangerMap::predict(bombs, grid, TickRate);

    EXPECT_TRUE(danger.isHitAt({3, 1}, 59)); // destroyed by the first bomb
    EXPECT_EQ(danger.freeFrom({3, 1}), 60);
    EXPECT_TRUE(danger.isHitAt({3, 1}, FreshBombTick)); // the second one goes through
    EXPECT_TRUE(danger.isHitAt({2, 1}, FreshBombTick));
    EXPECT_EQ(danger.hitsAt({3, 1}), (std::vector<int>{59, FreshBombTick}));
}

TEST(DangerMapTest, AnUnnoticedBombBlocksItsCellButIsNotPredicted)
{
    const Grid grid(7, 7);
    const DangerMap danger =
        DangerMap::predict({Bomb({3, 3})}, grid, TickRate, [](const Bomb&) { return false; });

    EXPECT_FALSE(danger.isThreatened({3, 2}));
    EXPECT_FALSE(danger.isThreatened({3, 3}));
    EXPECT_EQ(danger.freeFrom({3, 3}), DangerMap::Forever);
    EXPECT_EQ(danger.getCalmTick(), 0);
}

// The prediction must agree with what the BombManager actually does, tick
// by tick, including blocks destroyed by one bomb opening the way to the
// next.
TEST(DangerMapTest, PredictionsMatchTheBombManager)
{
    Grid grid(9, 7);
    for (int x = 0; x < 9; ++x)
    {
        grid.setTile({x, 0}, Tile(TileType::Wall));
        grid.setTile({x, 6}, Tile(TileType::Wall));
    }
    for (const Position block : {Position{3, 1}, Position{5, 3}, Position{2, 4}, Position{7, 2}, Position{4, 5}})
        grid.setTile(block, Tile(TileType::DestructibleBlock));
    grid.setTile({4, 3}, Tile(TileType::Wall));

    BombManager bombs;
    auto run = [&](Grid& board, int ticks) {
        for (int i = 0; i < ticks; ++i)
            (void)bombs.update(Tick, board);
    };
    ASSERT_TRUE(bombs.placeBomb({1, 1}, grid, 0, 5, 3));
    run(grid, 20);
    ASSERT_TRUE(bombs.placeBomb({6, 3}, grid, 1, 5, 2));
    run(grid, 30);
    ASSERT_TRUE(bombs.placeBomb({2, 3}, grid, 2, 5, 4));
    ASSERT_TRUE(bombs.placeBomb({7, 5}, grid, 3, 5, 1));
    run(grid, 10);

    const DangerMap danger = DangerMap::predict(bombs.getBombs(), grid, TickRate);

    Grid board = grid;
    for (int tick = 0; tick <= danger.getCalmTick() + 5; ++tick)
    {
        const std::vector<Position> hit = bombs.update(Tick, board);
        for (int y = 0; y < grid.getHeight(); ++y)
            for (int x = 0; x < grid.getWidth(); ++x)
            {
                const Position cell{x, y};
                const bool actual = std::find(hit.begin(), hit.end(), cell) != hit.end();
                EXPECT_EQ(danger.isHitAt(cell, tick), actual) << "cell (" << x << ", " << y << ") tick " << tick;
            }
    }
    EXPECT_TRUE(bombs.getBombs().empty());

    // Every block that was destroyed was predicted to open.
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
            if (!grid.isWalkable({x, y}) && board.isWalkable({x, y}))
                EXPECT_LT(danger.freeFrom({x, y}), DangerMap::Forever) << x << ", " << y;
}

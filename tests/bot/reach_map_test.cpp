#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bomb.h"
#include "danger_map.h"
#include "game_engine.h"
#include "grid.h"
#include "reach_map.h"

using Bomberman::Bomb;
using Bomberman::CharacterCommand;
using Bomberman::DangerMap;
using Bomberman::Direction;
using Bomberman::GameEngine;
using Bomberman::GameEvent;
using Bomberman::Grid;
using Bomberman::PlayerId;
using Bomberman::Position;
using Bomberman::ReachMap;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

constexpr int TickRate = 60;
constexpr float Tick = 1.f / static_cast<float>(TickRate);
constexpr int Move = 10; // GameRules::moveTicks

Bomb ticking(Position position, int ticks, int blastRadius = 1)
{
    Bomb bomb(position, Bomb::DefaultFuseDuration, blastRadius);
    for (int i = 0; i < ticks; ++i)
        bomb.update(Tick);
    return bomb;
}

ReachMap explore(const DangerMap& danger, Position start, int delay = 0)
{
    return ReachMap::explore(danger, start, delay, Move);
}

// A grid made of walls except for the '.' cells of rows.
Grid fromRows(const std::vector<std::string>& rows)
{
    Grid grid(static_cast<int>(rows.front().size()), static_cast<int>(rows.size()), TileType::Wall);
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
        {
            const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (c == '.')
                grid.setTile({x, y}, Tile(TileType::Empty));
            else if (c == '+')
                grid.setTile({x, y}, Tile(TileType::DestructibleBlock));
        }
    return grid;
}

} // namespace

TEST(ReachMapTest, WithoutDangerItIsAShortestPathOfImmediateMoves)
{
    const Grid grid(7, 7);
    const ReachMap reach = explore(DangerMap::predict({}, grid, TickRate), {1, 1});

    EXPECT_EQ(reach.getStart(), (Position{1, 1}));
    EXPECT_EQ(reach.safeArrival({1, 1}), 0);
    EXPECT_EQ(reach.firstStep({1, 1}), std::nullopt); // already there

    // No turn to pay any more: every step costs moveTicks.
    EXPECT_EQ(reach.safeArrival({3, 1}), 2 * Move);
    EXPECT_EQ(reach.firstStep({3, 1}), Direction::Right);
    EXPECT_EQ(reach.safeArrival({1, 3}), 2 * Move);
    EXPECT_EQ(reach.firstStep({1, 3}), Direction::Down);
    EXPECT_EQ(reach.safeArrival({3, 3}), 4 * Move);
    const std::optional<Direction> towardsCorner = reach.firstStep({3, 3});
    EXPECT_TRUE(towardsCorner == Direction::Down || towardsCorner == Direction::Right);
    EXPECT_GT(reach.getExploredStates(), 0);
}

TEST(ReachMapTest, WallsAndBlocksCannotBeCrossed)
{
    const Grid grid = fromRows({
        "#######",
        "#..#..#",
        "#..+..#",
        "#######",
    });
    const ReachMap reach = explore(DangerMap::predict({}, grid, TickRate), {1, 1});

    EXPECT_TRUE(reach.isSafelyReachable({2, 2}));
    EXPECT_FALSE(reach.isSafelyReachable({4, 1}));
    EXPECT_EQ(reach.safeArrival({4, 1}), ReachMap::Unreachable);
    EXPECT_EQ(reach.firstStep({4, 1}), std::nullopt);
    EXPECT_EQ(reach.safeArrival({-1, 1}), ReachMap::Unreachable);
}

TEST(ReachMapTest, TheWayOutOfABlastIsTheNearestSafeCell)
{
    const Grid grid(7, 7);
    const DangerMap danger = DangerMap::predict({Bomb({2, 1})}, grid, TickRate);
    const ReachMap reach = explore(danger, {1, 1});

    ASSERT_TRUE(reach.canSurvive());
    // One step up, down or left gets out of the cross: the first in
    // reading order wins.
    EXPECT_EQ(reach.nearestSafeCell(), (Position{1, 0}));
    EXPECT_EQ(reach.firstStep({1, 0}), Direction::Up);
    EXPECT_EQ(reach.safeArrival({1, 2}), Move);
    EXPECT_EQ(reach.firstStep({1, 2}), Direction::Down);
    // Its own cell is only safe for good once the bomb has exploded.
    EXPECT_GT(reach.safeArrival({1, 1}), DangerMap::explosionTick(Bomb({2, 1}), TickRate));
}

TEST(ReachMapTest, ADeadEndWithABombHasNoWayOut)
{
    const Grid grid = fromRows({
        "#####",
        "#...#",
        "#####",
    });
    const DangerMap danger = DangerMap::predict({Bomb({3, 1}, Bomb::DefaultFuseDuration, 3)}, grid, TickRate);
    const ReachMap reach = explore(danger, {1, 1});

    EXPECT_FALSE(reach.canSurvive());
    EXPECT_EQ(reach.nearestSafeCell(), std::nullopt);
}

// The only way out crosses a cell that explodes in 3 ticks: walking now
// would stand on it at that moment, so the best is to wait for the
// explosion to pass (tick 3) and go at tick 4.
TEST(ReachMapTest, WaitingLetsAnExplosionPassFirst)
{
    const Grid grid = fromRows({
        "########",
        "#......#",
        "##.#####",
        "##.#####",
        "########",
    });
    const Bomb bomb = ticking({2, 3}, 116, 2); // explodes at tick 3, hits (2, 1)
    ASSERT_EQ(DangerMap::explosionTick(bomb, TickRate), 3);
    const DangerMap danger = DangerMap::predict({bomb}, grid, TickRate);
    ASSERT_TRUE(danger.isHitAt({2, 1}, 3));
    ASSERT_FALSE(danger.isThreatened({1, 1}));

    const ReachMap reach = explore(danger, {1, 1});

    EXPECT_EQ(reach.safeArrival({6, 1}), 4 + 5 * Move);
    EXPECT_EQ(reach.firstStep({6, 1}), std::nullopt); // wait
}

TEST(ReachMapTest, ACellClearedByAnExplosionCanBeCrossedAfterwards)
{
    const Grid grid = fromRows({
        "#######",
        "#..+..#",
        "#######",
    });
    const Bomb bomb = ticking({5, 1}, 110, 2); // explodes at tick 9, destroys (3, 1)
    const DangerMap danger = DangerMap::predict({bomb}, grid, TickRate);
    ASSERT_EQ(danger.freeFrom({3, 1}), 10);

    const ReachMap reach = explore(danger, {1, 1});

    EXPECT_EQ(reach.safeArrival({3, 1}), 2 * Move); // (2, 1) at tick 0, (3, 1) at tick 10
    EXPECT_EQ(reach.safeArrival({4, 1}), 3 * Move);
    EXPECT_EQ(reach.safeArrival({5, 1}), 4 * Move); // where the bomb was
}

// The character dropped a bomb where it stands: it can step off it, but
// not come back before the explosion.
TEST(ReachMapTest, ACharacterLeavesItsOwnBombButCannotComeBack)
{
    const Grid grid(7, 7);
    const DangerMap danger = DangerMap::predict({Bomb({3, 3})}, grid, TickRate);
    const ReachMap reach = explore(danger, {3, 3}, 15); // right after dropping it

    ASSERT_TRUE(reach.canSurvive());
    EXPECT_EQ(reach.safeArrival({2, 2}), 15 + 2 * Move); // out of the cross in two steps
    EXPECT_GT(reach.safeArrival({3, 3}), DangerMap::explosionTick(Bomb({3, 3}), TickRate));
}

TEST(ReachMapTest, TheStartCellMustSurviveTheDelay)
{
    const Grid grid(7, 7);

    const DangerMap later = DangerMap::predict({Bomb({2, 1})}, grid, TickRate);
    EXPECT_TRUE(explore(later, {1, 1}, 15).canSurvive());

    const DangerMap soon = DangerMap::predict({ticking({2, 1}, 110)}, grid, TickRate); // tick 9
    const ReachMap reach = explore(soon, {1, 1}, 15);
    EXPECT_FALSE(reach.canSurvive());
    EXPECT_EQ(reach.safeArrival({1, 1}), ReachMap::Unreachable);
}

// Following the first steps in the real engine gets the character out of
// its own bomb's blast.
TEST(ReachMapTest, FollowingTheFirstStepsInTheEngineEscapesTheBomb)
{
    Grid grid(7, 7);
    grid.addSpawnPoint({1, 1});
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    engine.submitCommand(id, CharacterCommand::placeBomb()); // on (1, 1), under the character
    engine.tick();
    ASSERT_EQ(engine.getBombManager().getBombs().size(), 1u);

    bool exploded = false;
    for (int tick = 0; tick < 150 && !exploded; ++tick)
    {
        if (engine.isReady(id))
        {
            const Bomberman::Character& character = engine.getCharacter(id);
            const DangerMap danger =
                DangerMap::predict(engine.getBombManager().getBombs(), engine.getGrid(), TickRate);
            const ReachMap reach = explore(danger, character.getPosition());
            const std::optional<Position> safe = reach.nearestSafeCell();
            ASSERT_TRUE(safe.has_value());
            if (const std::optional<Direction> dir = reach.firstStep(*safe))
                engine.submitCommand(id, CharacterCommand::move(*dir));
        }
        engine.tick();
        for (const GameEvent& event : engine.takeEvents())
            exploded = exploded || event.type == GameEvent::Type::Explosion;
    }

    EXPECT_TRUE(exploded);
    EXPECT_TRUE(engine.getCharacter(id).isAlive());
}

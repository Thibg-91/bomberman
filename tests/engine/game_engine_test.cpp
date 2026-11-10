#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "game_engine.h"

using Bomberman::Bomb;
using Bomberman::CharacterCommand;
using Bomberman::Direction;
using Bomberman::GameEngine;
using Bomberman::GameEvent;
using Bomberman::GameRules;
using Bomberman::Grid;
using Bomberman::PlayerId;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

constexpr int FuseTicks = 120; // Bomb::DefaultFuseDuration (2 s) at 60 Hz

// Walkable 7x7 board with spawn points at (1, 1) and (5, 5); tests add
// obstacles.
Grid openGrid()
{
    Grid grid(7, 7);
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({5, 5});
    return grid;
}

void runTicks(GameEngine& engine, int ticks)
{
    for (int i = 0; i < ticks; ++i)
        engine.tick();
}

// Submits command and runs one tick.
void act(GameEngine& engine, PlayerId id, const CharacterCommand& command)
{
    engine.submitCommand(id, command);
    engine.tick();
}

// Turns the player towards direction and waits until it can act again.
void face(GameEngine& engine, PlayerId id, Direction direction)
{
    act(engine, id, CharacterCommand::move(direction));
    ASSERT_EQ(engine.getCharacter(id).getFacing(), direction);
    runTicks(engine, engine.getCooldownTicks(id));
    ASSERT_TRUE(engine.isReady(id));
}

int countEvents(const std::vector<GameEvent>& events, GameEvent::Type type)
{
    return static_cast<int>(
        std::count_if(events.begin(), events.end(), [type](const GameEvent& event) { return event.type == type; }));
}

const GameEvent* findEvent(const std::vector<GameEvent>& events, GameEvent::Type type)
{
    const auto it =
        std::find_if(events.begin(), events.end(), [type](const GameEvent& event) { return event.type == type; });
    return it == events.end() ? nullptr : &*it;
}

} // namespace

// =========================
// Rules
// =========================
TEST(GameRulesTest, DefaultsRunAt60HzWithASlightDelayAfterEveryAction)
{
    const GameRules rules;

    EXPECT_EQ(rules.tickRate, 60);
    EXPECT_FLOAT_EQ(rules.tickDuration(), 1.f / 60.f);
    EXPECT_FLOAT_EQ(rules.secondsFor(60), 1.f);
    EXPECT_GT(rules.moveTicks, 0);
    EXPECT_GT(rules.turnTicks, 0);
    EXPECT_GT(rules.placeBombTicks, 0);
    EXPECT_GT(rules.pushBombTicks, 0);
    // A short pause, not a sluggish one.
    EXPECT_LE(rules.secondsFor(rules.placeBombTicks), 0.5f);
}

// =========================
// Players
// =========================
TEST(GameEngineTest, StartsWithoutPlayersAtTickZero)
{
    const GameEngine engine(openGrid());

    EXPECT_EQ(engine.getTick(), 0u);
    EXPECT_EQ(engine.getPlayerCount(), 0);
    EXPECT_EQ(engine.countAlivePlayers(), 0);
    EXPECT_FALSE(engine.hasPlayer(0));
    EXPECT_TRUE(engine.getBombManager().getBombs().empty());
}

TEST(GameEngineTest, PlayersSpawnOnTheSpawnPointsInOrder)
{
    GameEngine engine(openGrid());

    EXPECT_EQ(engine.addPlayer(), PlayerId{0});
    EXPECT_EQ(engine.addPlayer(), PlayerId{1});

    EXPECT_EQ(engine.getPlayerCount(), 2);
    EXPECT_EQ(engine.countAlivePlayers(), 2);
    EXPECT_EQ(engine.getCharacter(0).getPosition(), (Position{1, 1}));
    EXPECT_EQ(engine.getCharacter(1).getPosition(), (Position{5, 5}));
    EXPECT_TRUE(engine.isReady(0));
    EXPECT_TRUE(engine.isReady(1));
}

TEST(GameEngineTest, NoMorePlayersThanSpawnPoints)
{
    GameEngine engine(openGrid());
    ASSERT_TRUE(engine.addPlayer());
    ASSERT_TRUE(engine.addPlayer());

    EXPECT_FALSE(engine.addPlayer().has_value());
    EXPECT_EQ(engine.getPlayerCount(), 2);
}

TEST(GameEngineTest, UnknownPlayersAreRejected)
{
    GameEngine engine(openGrid());
    ASSERT_TRUE(engine.addPlayer());

    EXPECT_THROW((void)engine.getCharacter(1), std::out_of_range);
    EXPECT_THROW((void)engine.getCharacter(-1), std::out_of_range);
    EXPECT_THROW((void)engine.isReady(3), std::out_of_range);
    EXPECT_THROW((void)engine.getCooldownTicks(3), std::out_of_range);
    EXPECT_NO_THROW(engine.submitCommand(7, CharacterCommand::placeBomb()));
    engine.tick();
    EXPECT_TRUE(engine.getBombManager().getBombs().empty());
}

TEST(GameEngineTest, TheWalkDurationMatchesTheMoveDelay)
{
    GameRules rules;
    rules.moveTicks = 12;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();

    EXPECT_FLOAT_EQ(engine.getCharacter(id).getMoveDuration(), rules.secondsFor(12));
}

// =========================
// Commands and pacing
// =========================
TEST(GameEngineTest, CommandsAreOnlyExecutedByTheNextTick)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();

    engine.submitCommand(id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Down);

    engine.tick();
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Right);
    EXPECT_EQ(engine.getTick(), 1u);
}

TEST(GameEngineTest, TheLatestSubmissionBeforeATickWins)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();

    engine.submitCommand(id, CharacterCommand::move(Direction::Right));
    engine.submitCommand(id, CharacterCommand::move(Direction::Up));
    engine.tick();

    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Up);
}

TEST(GameEngineTest, TurningTakesTurnTicksBeforeTheNextAction)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();

    act(engine, id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 1})); // only turned
    EXPECT_FALSE(engine.isReady(id));

    // The cooldown started on the tick of the turn: ready again turnTicks
    // ticks after it.
    runTicks(engine, rules.turnTicks - 1);
    EXPECT_TRUE(engine.isReady(id));
    EXPECT_EQ(engine.getCooldownTicks(id), 0);
}

TEST(GameEngineTest, CommandsReceivedWhileBusyAreDropped)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::move(Direction::Right)); // turn

    // Submitted while still turning: lost, not replayed later.
    act(engine, id, CharacterCommand::move(Direction::Right));
    runTicks(engine, rules.turnTicks);

    EXPECT_TRUE(engine.isReady(id));
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 1}));
}

TEST(GameEngineTest, MovingOneCellTakesMoveTicks)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{2, 1}));
    EXPECT_EQ(engine.getCooldownTicks(id), rules.moveTicks - 1);

    runTicks(engine, rules.moveTicks - 2);
    EXPECT_FALSE(engine.isReady(id));
    EXPECT_TRUE(engine.getCharacter(id).isMoving());

    engine.tick();
    EXPECT_TRUE(engine.isReady(id));
    EXPECT_FALSE(engine.getCharacter(id).isMoving());
}

TEST(GameEngineTest, HoldingADirectionWalksOneCellEveryMoveTicks)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);

    // What the keyboard does while the key is held: the same command every
    // tick. Only the ones arriving when the player is ready count.
    for (int i = 0; i < 3 * rules.moveTicks; ++i)
        act(engine, id, CharacterCommand::move(Direction::Right));

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{4, 1}));
    EXPECT_TRUE(engine.isReady(id));
}

TEST(GameEngineTest, FailedActionsCostNoDelay)
{
    Grid grid = openGrid();
    grid.setTile({2, 1}, Tile(TileType::Wall));
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::move(Direction::Right)); // blocked by the wall
    EXPECT_TRUE(engine.isReady(id));

    act(engine, id, CharacterCommand::placeBomb()); // the faced cell is a wall
    EXPECT_TRUE(engine.isReady(id));

    act(engine, id, CharacterCommand::pushBomb()); // no bomb to push
    EXPECT_TRUE(engine.isReady(id));
    EXPECT_EQ(engine.takeEvents().size(), 1u); // only the turn
}

// =========================
// Bombs
// =========================
TEST(GameEngineTest, ABombIsDroppedOnTheFacedCellThenThePlayerPauses)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer(); // (1, 1), facing down
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::placeBomb());

    ASSERT_EQ(engine.getBombManager().getBombs().size(), 1u);
    const Bomb& bomb = engine.getBombManager().getBombs().front();
    EXPECT_EQ(bomb.getPosition(), (Position{1, 2}));
    EXPECT_EQ(bomb.getOwnerId(), id);
    EXPECT_FALSE(engine.isReady(id));
    EXPECT_EQ(engine.getCooldownTicks(id), rules.placeBombTicks - 1);

    const std::vector<GameEvent> events = engine.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, GameEvent::Type::BombPlaced);
    EXPECT_EQ(events[0].playerId, id);
    EXPECT_EQ(events[0].position, (Position{1, 2}));
    EXPECT_EQ(events[0].tick, 0u);
}

TEST(GameEngineTest, OneActiveBombPerPlayerByDefault)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb());
    runTicks(engine, rules.placeBombTicks);
    face(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::placeBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().size(), 1u);
}

TEST(GameEngineTest, MaxActiveBombsComesFromTheRules)
{
    GameRules rules;
    rules.startingStats.maxBombs = 2;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb());
    runTicks(engine, rules.placeBombTicks);
    face(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::placeBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().size(), 2u);
}

TEST(GameEngineTest, BombsBlockTheWay)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2)
    runTicks(engine, rules.placeBombTicks);

    act(engine, id, CharacterCommand::move(Direction::Down));

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 1}));
}

TEST(GameEngineTest, PushingABombMovesItAndPausesThePlayer)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2)
    runTicks(engine, rules.placeBombTicks);
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::pushBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().front().getPosition(), (Position{1, 3}));
    EXPECT_EQ(engine.getCooldownTicks(id), rules.pushBombTicks - 1);
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* pushed = findEvent(events, GameEvent::Type::BombPushed);
    ASSERT_NE(pushed, nullptr);
    EXPECT_EQ(pushed->position, (Position{1, 3}));
}

TEST(GameEngineTest, ABombExplodesAfterExactlyItsFuseInTicks)
{
    Grid grid = openGrid();
    grid.setTile({1, 3}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);
    act(engine, id, CharacterCommand::placeBomb()); // on (2, 1), placed during this tick
    (void)engine.takeEvents();

    // The tick where the bomb was dropped already burnt its fuse once.
    runTicks(engine, FuseTicks - 2);
    EXPECT_EQ(engine.getBombManager().getBombs().size(), 1u);
    EXPECT_EQ(countEvents(engine.takeEvents(), GameEvent::Type::Explosion), 0);

    engine.tick();
    EXPECT_TRUE(engine.getBombManager().getBombs().empty());
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* explosion = findEvent(events, GameEvent::Type::Explosion);
    ASSERT_NE(explosion, nullptr);
    EXPECT_EQ(explosion->playerId, -1);
    EXPECT_NE(std::find(explosion->cells.begin(), explosion->cells.end(), Position{2, 1}), explosion->cells.end());
}

TEST(GameEngineTest, ExplosionsDestroyBlocksInTheEngineGrid)
{
    Grid grid = openGrid();
    grid.setTile({1, 3}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2), next to the block

    runTicks(engine, FuseTicks);

    EXPECT_TRUE(engine.getGrid().isWalkable({1, 3}));
}

TEST(GameEngineTest, PlayersInTheBlastAreKilledAndStopActing)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2), (1, 1) is in its blast
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks);

    EXPECT_FALSE(engine.getCharacter(id).isAlive());
    EXPECT_EQ(engine.countAlivePlayers(), 0);
    EXPECT_FALSE(engine.isReady(id));

    // Explosion first, then its victims.
    const std::vector<GameEvent> events = engine.takeEvents();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].type, GameEvent::Type::Explosion);
    EXPECT_EQ(events[1].type, GameEvent::Type::PlayerKilled);
    EXPECT_EQ(events[1].playerId, id);

    act(engine, id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Down);
}

TEST(GameEngineTest, PlayersOutOfTheBlastSurvive)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);
    act(engine, id, CharacterCommand::placeBomb()); // on (2, 1)
    runTicks(engine, rules.placeBombTicks);

    // Run away: down twice (turn first), out of the cross of (2, 1).
    face(engine, id, Direction::Down);
    for (int i = 0; i < 2 * rules.moveTicks; ++i)
        act(engine, id, CharacterCommand::move(Direction::Down));
    ASSERT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 3}));

    runTicks(engine, FuseTicks);

    EXPECT_TRUE(engine.getBombManager().getBombs().empty());
    EXPECT_TRUE(engine.getCharacter(id).isAlive());
}

// =========================
// Several players (AI / network ready)
// =========================
TEST(GameEngineTest, PlayersActIndependentlyOnTheSameTick)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId first = *engine.addPlayer();
    const PlayerId second = *engine.addPlayer();

    engine.submitCommand(first, CharacterCommand::placeBomb());
    engine.submitCommand(second, CharacterCommand::move(Direction::Up));
    engine.tick();

    EXPECT_EQ(engine.getBombManager().countActiveBombs(first), 1);
    EXPECT_EQ(engine.getCharacter(second).getFacing(), Direction::Up);
    EXPECT_EQ(engine.getCooldownTicks(first), rules.placeBombTicks - 1);
    EXPECT_EQ(engine.getCooldownTicks(second), rules.turnTicks - 1);

    // The second player's own cooldown is shorter: it can act while the
    // first one is still busy.
    runTicks(engine, rules.turnTicks - 1);
    EXPECT_TRUE(engine.isReady(second));
    EXPECT_FALSE(engine.isReady(first));
    act(engine, second, CharacterCommand::placeBomb());
    EXPECT_EQ(engine.getBombManager().countActiveBombs(second), 1);
}

TEST(GameEngineTest, ABombKillsEveryPlayerInItsBlast)
{
    Grid grid(7, 7);
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({3, 1});
    GameEngine engine(std::move(grid));
    const PlayerId first = *engine.addPlayer();
    const PlayerId second = *engine.addPlayer();
    face(engine, first, Direction::Right);
    act(engine, first, CharacterCommand::placeBomb()); // on (2, 1), between both

    runTicks(engine, FuseTicks);

    EXPECT_FALSE(engine.getCharacter(first).isAlive());
    EXPECT_FALSE(engine.getCharacter(second).isAlive());
    EXPECT_EQ(countEvents(engine.takeEvents(), GameEvent::Type::PlayerKilled), 2);
}

// =========================
// Real time
// =========================
TEST(GameEngineTest, AdvanceRunsTicksFromRealTime)
{
    GameEngine engine(openGrid());

    EXPECT_EQ(engine.advance(0.5f / 60.f), 0);
    EXPECT_NEAR(engine.getInterpolationAlpha(), 0.5f, 1e-4f);
    EXPECT_EQ(engine.advance(0.5f / 60.f), 1);
    EXPECT_EQ(engine.advance(2.f / 60.f), 2);
    EXPECT_EQ(engine.getTick(), 3u);
}

// The same inputs give the same game whatever the frame rate: what matters
// is the tick, not the frame.
class GameEngineFrameRateTest : public ::testing::TestWithParam<int>
{
};

TEST_P(GameEngineFrameRateTest, TheGameDoesNotDependOnTheFrameRate)
{
    const int framesPerSecond = GetParam();
    const float frame = 1.f / static_cast<float>(framesPerSecond);
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();

    // Hold "right" for half a second (30 ticks), as main.cpp does: the
    // command is submitted every frame. The first press only turns the
    // character (5 ticks), then it starts a 10-tick walk on ticks 5, 15 and
    // 25.
    for (int i = 0; i < framesPerSecond / 2; ++i)
    {
        engine.submitCommand(id, CharacterCommand::move(Direction::Right));
        engine.advance(frame);
    }

    EXPECT_EQ(engine.getTick(), 30u);
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{4, 1}));
}

INSTANTIATE_TEST_SUITE_P(FrameRates, GameEngineFrameRateTest, ::testing::Values(30, 60, 144));

TEST(GameEngineTest, AFrameCommandStaysPendingForAllTheTicksOfThatFrame)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::move(Direction::Right)); // turn on tick 0
    runTicks(engine, rules.turnTicks - 2);                     // ticks 1..3, ready on tick 5

    // One slow frame covering ticks 4 and 5: the player becomes ready
    // during it and still gets the command.
    engine.submitCommand(id, CharacterCommand::move(Direction::Right));
    ASSERT_EQ(engine.advance(2.f / 60.f), 2);

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{2, 1}));
}

TEST(GameEngineTest, AFrameWithoutTickKeepsItsCommandForTheNextOne)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();

    engine.submitCommand(id, CharacterCommand::move(Direction::Right));
    ASSERT_EQ(engine.advance(0.5f / 60.f), 0);
    ASSERT_EQ(engine.advance(0.5f / 60.f), 1);

    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Right);
}

TEST(GameEngineTest, EventsAreTimestampedAndClearedOnceTaken)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    runTicks(engine, 3);

    act(engine, id, CharacterCommand::move(Direction::Right));

    std::vector<GameEvent> events = engine.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, GameEvent::Type::PlayerTurned);
    EXPECT_EQ(events[0].tick, 3u);
    EXPECT_TRUE(engine.takeEvents().empty());
}

// =========================
// Upgrades
// =========================
namespace
{

// Every destroyed block hides an upgrade of the given type.
GameRules alwaysDrop(Bomberman::UpgradeType type)
{
    GameRules rules;
    rules.upgrades.dropChancePercent = 100;
    rules.upgrades.weights = {0, 0, 0};
    rules.upgrades.weights[static_cast<std::size_t>(Bomberman::indexOf(type))] = 1;
    return rules;
}

// Turns right then walks one cell right, from (1, 1) to (2, 1).
void walkRight(GameEngine& engine, PlayerId id)
{
    face(engine, id, Direction::Right);
    act(engine, id, CharacterCommand::move(Direction::Right));
    runTicks(engine, engine.getCooldownTicks(id));
}

} // namespace

using Bomberman::Upgrade;
using Bomberman::UpgradeType;

TEST(GameEngineUpgradeTest, PlayersStartWithTheRulesStartingStats)
{
    GameRules rules;
    rules.startingStats = {2, 3, 4};
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();

    EXPECT_EQ(engine.getStats(id), (Bomberman::PlayerStats{2, 3, 4}));
    EXPECT_THROW((void)engine.getStats(5), std::out_of_range);
}

TEST(GameEngineUpgradeTest, ADestroyedBlockCanRevealAnUpgrade)
{
    Grid grid = openGrid();
    grid.setTile({1, 3}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid), alwaysDrop(UpgradeType::BlastRadius));
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2), next to the block
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks);

    // The explosion that revealed it spares it.
    ASSERT_EQ(engine.getUpgrades().size(), 1u);
    EXPECT_EQ(engine.getUpgrades().front(), (Upgrade{{1, 3}, UpgradeType::BlastRadius}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* spawned = findEvent(events, GameEvent::Type::UpgradeSpawned);
    ASSERT_NE(spawned, nullptr);
    EXPECT_EQ(spawned->position, (Position{1, 3}));
    EXPECT_EQ(spawned->upgrade, UpgradeType::BlastRadius);
    EXPECT_EQ(countEvents(events, GameEvent::Type::UpgradeDestroyed), 0);
}

TEST(GameEngineUpgradeTest, NoUpgradeWhenTheDropChanceIsZero)
{
    Grid grid = openGrid();
    grid.setTile({1, 3}, Tile(TileType::DestructibleBlock));
    GameRules rules;
    rules.upgrades.dropChancePercent = 0;
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb());

    runTicks(engine, FuseTicks);

    EXPECT_TRUE(engine.getGrid().isWalkable({1, 3}));
    EXPECT_TRUE(engine.getUpgrades().empty());
}

TEST(GameEngineUpgradeTest, AddUpgradeNeedsAFreeWalkableCell)
{
    Grid grid = openGrid();
    grid.setTile({3, 3}, Tile(TileType::Wall));
    GameEngine engine(std::move(grid));

    EXPECT_TRUE(engine.addUpgrade({{2, 2}, UpgradeType::ExtraBomb}));
    EXPECT_FALSE(engine.addUpgrade({{2, 2}, UpgradeType::BlastRadius}));
    EXPECT_FALSE(engine.addUpgrade({{3, 3}, UpgradeType::ExtraBomb}));
    EXPECT_EQ(engine.getUpgrades().size(), 1u);
}

TEST(GameEngineUpgradeTest, WalkingOnAnUpgradeCollectsItAtOnce)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::ExtraBomb}));
    face(engine, id, Direction::Right);
    (void)engine.takeEvents();

    // Collected on the tick the move starts, not when the walk ends.
    act(engine, id, CharacterCommand::move(Direction::Right));

    EXPECT_TRUE(engine.getUpgrades().empty());
    EXPECT_EQ(engine.getStats(id).maxBombs, 2);
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* collected = findEvent(events, GameEvent::Type::UpgradeCollected);
    ASSERT_NE(collected, nullptr);
    EXPECT_EQ(collected->playerId, id);
    EXPECT_EQ(collected->position, (Position{2, 1}));
    EXPECT_EQ(collected->upgrade, UpgradeType::ExtraBomb);
}

TEST(GameEngineUpgradeTest, ExtraBombLetsThePlayerDropOneMoreBomb)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::ExtraBomb}));
    walkRight(engine, id); // now on (2, 1), facing right

    act(engine, id, CharacterCommand::placeBomb()); // on (3, 1)
    runTicks(engine, rules.placeBombTicks);
    face(engine, id, Direction::Down);
    act(engine, id, CharacterCommand::placeBomb()); // on (2, 2)
    runTicks(engine, rules.placeBombTicks);
    face(engine, id, Direction::Left);
    act(engine, id, CharacterCommand::placeBomb()); // third one: refused

    EXPECT_EQ(engine.getBombManager().countActiveBombs(id), 2);
}

TEST(GameEngineUpgradeTest, BlastRadiusMakesTheNextBombsReachFurther)
{
    Grid grid = openGrid();
    grid.setTile({5, 1}, Tile(TileType::DestructibleBlock)); // 2 cells from the bomb
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::BlastRadius}));
    walkRight(engine, id);
    ASSERT_EQ(engine.getStats(id).blastRadius, 2);

    act(engine, id, CharacterCommand::placeBomb()); // on (3, 1)
    EXPECT_EQ(engine.getBombManager().getBombs().front().getBlastRadius(), 2);

    runTicks(engine, FuseTicks);
    EXPECT_TRUE(engine.getGrid().isWalkable({5, 1}));
}

TEST(GameEngineUpgradeTest, PushStrengthMakesBombsSlideFurther)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::PushStrength}));
    walkRight(engine, id);
    ASSERT_EQ(engine.getStats(id).pushDistance, 2);
    act(engine, id, CharacterCommand::placeBomb()); // on (3, 1)
    runTicks(engine, rules.placeBombTicks);
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::pushBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().front().getPosition(), (Position{5, 1}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* pushed = findEvent(events, GameEvent::Type::BombPushed);
    ASSERT_NE(pushed, nullptr);
    EXPECT_EQ(pushed->position, (Position{5, 1}));
}

TEST(GameEngineUpgradeTest, APushedBombStopsBeforeAnotherPlayer)
{
    Grid grid(9, 3);
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({5, 1});
    GameRules rules;
    rules.startingStats.pushDistance = 5;
    GameEngine engine(std::move(grid), rules);
    const PlayerId pusher = *engine.addPlayer();
    (void)engine.addPlayer();
    face(engine, pusher, Direction::Right);
    act(engine, pusher, CharacterCommand::placeBomb()); // on (2, 1)
    runTicks(engine, rules.placeBombTicks);

    act(engine, pusher, CharacterCommand::pushBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().front().getPosition(), (Position{4, 1}));
}

TEST(GameEngineUpgradeTest, AnUpgradeAtTheCapIsConsumedWithoutEffect)
{
    GameRules rules;
    rules.upgrades.maxBombs = 1;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::ExtraBomb}));

    walkRight(engine, id);

    EXPECT_TRUE(engine.getUpgrades().empty());
    EXPECT_EQ(engine.getStats(id).maxBombs, 1);
}

TEST(GameEngineUpgradeTest, ExplosionsDestroyTheUpgradesInTheirBlast)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{1, 3}, UpgradeType::PushStrength})); // in the blast of (1, 2)
    ASSERT_TRUE(engine.addUpgrade({{4, 4}, UpgradeType::ExtraBomb}));    // out of it
    act(engine, id, CharacterCommand::placeBomb());                      // on (1, 2)
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks);

    ASSERT_EQ(engine.getUpgrades().size(), 1u);
    EXPECT_EQ(engine.getUpgrades().front().position, (Position{4, 4}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* destroyed = findEvent(events, GameEvent::Type::UpgradeDestroyed);
    ASSERT_NE(destroyed, nullptr);
    EXPECT_EQ(destroyed->position, (Position{1, 3}));
    EXPECT_EQ(destroyed->upgrade, UpgradeType::PushStrength);
}

TEST(GameEngineUpgradeTest, UpgradesDoNotBlockTheWay)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{1, 2}, UpgradeType::ExtraBomb}));

    act(engine, id, CharacterCommand::move(Direction::Down)); // already facing down

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 2}));
}

TEST(GameEngineUpgradeTest, DeadPlayersDoNotCollect)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 2), kills the player on (1, 1)
    runTicks(engine, FuseTicks);
    ASSERT_FALSE(engine.getCharacter(id).isAlive());

    ASSERT_TRUE(engine.addUpgrade({{1, 1}, UpgradeType::ExtraBomb}));
    engine.tick();

    EXPECT_EQ(engine.getUpgrades().size(), 1u);
}

namespace
{

// A bomb in the middle of 4 blocks; returns the upgrades they revealed.
std::vector<Upgrade> upgradesRevealedWithSeed(std::uint64_t seed)
{
    Grid grid(7, 7);
    grid.addSpawnPoint({3, 2});
    for (const Position block : {Position{2, 3}, Position{4, 3}, Position{3, 4}, Position{1, 2}})
        grid.setTile(block, Tile(TileType::DestructibleBlock));
    GameRules rules;
    rules.seed = seed;
    rules.upgrades.dropChancePercent = 50;
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (3, 3)
    runTicks(engine, FuseTicks);
    return engine.getUpgrades();
}

} // namespace

TEST(GameEngineUpgradeTest, UpgradesOnlyDependOnTheSeed)
{
    bool seedsDiffer = false;
    const std::vector<Upgrade> reference = upgradesRevealedWithSeed(1);
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const std::vector<Upgrade> upgrades = upgradesRevealedWithSeed(seed);
        EXPECT_EQ(upgrades, upgradesRevealedWithSeed(seed)) << "seed " << seed;
        seedsDiffer = seedsDiffer || upgrades != reference;
    }
    EXPECT_TRUE(seedsDiffer);
}

// =========================
// Explosion events (drawn by ExplosionRenderer)
// =========================
TEST(GameEngineTest, EachExplodingBombReportsItsBlast)
{
    Grid grid(9, 9);
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({7, 7});
    grid.setTile({1, 4}, Tile(TileType::DestructibleBlock));
    GameRules rules;
    rules.startingStats.blastRadius = 3;
    GameEngine engine(std::move(grid), rules);
    const PlayerId first = *engine.addPlayer();
    const PlayerId second = *engine.addPlayer();
    engine.submitCommand(first, CharacterCommand::placeBomb());  // on (1, 2)
    engine.submitCommand(second, CharacterCommand::placeBomb()); // on (7, 8)
    engine.tick();
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks - 1);

    std::vector<GameEvent> explosions;
    for (const GameEvent& event : engine.takeEvents())
        if (event.type == GameEvent::Type::Explosion)
            explosions.push_back(event);
    ASSERT_EQ(explosions.size(), 2u);

    const GameEvent& mine = explosions[0];
    EXPECT_EQ(mine.position, (Position{1, 2}));
    EXPECT_EQ(mine.blast.center, (Position{1, 2}));
    EXPECT_EQ(mine.blast.reachTowards(Direction::Down), 2); // hits the block on (1, 4)
    EXPECT_EQ(mine.blast.reachTowards(Direction::Up), 2);   // up to the grid edge
    EXPECT_EQ(mine.cells, mine.blast.cells());

    EXPECT_EQ(explosions[1].blast.center, (Position{7, 8}));
}

// =========================
// Controllers (bots)
// =========================
namespace
{

// Replays a fixed list of commands and records when it is asked.
class ScriptedController : public Bomberman::PlayerController
{
public:
    explicit ScriptedController(std::vector<CharacterCommand> script, std::vector<std::uint64_t>* askedAt = nullptr)
        : m_script(std::move(script)),
          m_askedAt(askedAt)
    {
    }

    std::optional<CharacterCommand> decide(const GameEngine& engine, PlayerId id) override
    {
        EXPECT_TRUE(engine.isReady(id)); // only asked when it can act
        if (m_askedAt)
            m_askedAt->push_back(engine.getTick());
        if (m_next >= m_script.size())
            return std::nullopt;
        return m_script[m_next++];
    }

private:
    std::vector<CharacterCommand> m_script;
    std::vector<std::uint64_t>* m_askedAt;
    std::size_t m_next = 0;
};

} // namespace

TEST(GameEngineControllerTest, PlayersHaveNoControllerByDefault)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();

    EXPECT_FALSE(engine.hasController(id));
    EXPECT_THROW((void)engine.hasController(5), std::out_of_range);
}

TEST(GameEngineControllerTest, AControllerIsOnlyAskedWhenItsPlayerIsReady)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    std::vector<std::uint64_t> askedAt;
    // Face right (5 ticks), walk right (10 ticks), then idle.
    engine.setController(id, std::make_unique<ScriptedController>(
                                 std::vector<CharacterCommand>{CharacterCommand::move(Direction::Right),
                                                               CharacterCommand::move(Direction::Right)},
                                 &askedAt));
    ASSERT_TRUE(engine.hasController(id));

    runTicks(engine, 20);

    // Asked at tick 0 (turn), 5 (walk), then every tick from 15 on.
    const std::vector<std::uint64_t> expected = {0, 5, 15, 16, 17, 18, 19};
    EXPECT_EQ(askedAt, expected);
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{2, 1}));
}

TEST(GameEngineControllerTest, AControlledPlayerIgnoresSubmittedCommands)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    engine.setController(id, std::make_unique<ScriptedController>(std::vector<CharacterCommand>{}));

    act(engine, id, CharacterCommand::move(Direction::Right));
    act(engine, id, CharacterCommand::placeBomb());

    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Down);
    EXPECT_TRUE(engine.getBombManager().getBombs().empty());
}

TEST(GameEngineControllerTest, RemovingTheControllerGivesThePlayerBackToCommands)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    engine.setController(id, std::make_unique<ScriptedController>(std::vector<CharacterCommand>{}));
    engine.setController(id, nullptr);

    EXPECT_FALSE(engine.hasController(id));
    act(engine, id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Right);
}

TEST(GameEngineControllerTest, ControllersAndCommandsDriveDifferentPlayers)
{
    GameEngine engine(openGrid());
    const PlayerId human = *engine.addPlayer();
    const PlayerId bot = *engine.addPlayer();
    engine.setController(bot, std::make_unique<ScriptedController>(
                                  std::vector<CharacterCommand>{CharacterCommand::placeBomb()}));
    engine.setController(42, std::make_unique<ScriptedController>(std::vector<CharacterCommand>{})); // ignored

    act(engine, human, CharacterCommand::move(Direction::Right));

    EXPECT_EQ(engine.getCharacter(human).getFacing(), Direction::Right);
    EXPECT_TRUE(engine.getBombManager().hasBombAt({5, 6})); // the bot (on (5, 5), facing down) dropped it
    EXPECT_EQ(engine.getBombManager().getBombs().front().getOwnerId(), bot);
}

TEST(GameEngineControllerTest, ADeadPlayerIsNoLongerAsked)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    const PlayerId bomber = *engine.addPlayer();
    std::vector<std::uint64_t> askedAt;
    engine.setController(id, std::make_unique<ScriptedController>(std::vector<CharacterCommand>{}, &askedAt));

    // The other player walks next to (1, 1) and blows it up.
    face(engine, bomber, Direction::Up);
    for (int i = 0; i < 3; ++i)
    {
        act(engine, bomber, CharacterCommand::move(Direction::Up));
        runTicks(engine, engine.getCooldownTicks(bomber));
    }
    face(engine, bomber, Direction::Left); // on (5, 2), facing (4, 2)
    for (int i = 0; i < 3; ++i)
    {
        act(engine, bomber, CharacterCommand::move(Direction::Left));
        runTicks(engine, engine.getCooldownTicks(bomber));
    }
    // On (2, 2): face up to drop the bomb on (2, 1), next to (1, 1).
    face(engine, bomber, Direction::Up);
    act(engine, bomber, CharacterCommand::placeBomb());
    runTicks(engine, FuseTicks);
    ASSERT_FALSE(engine.getCharacter(id).isAlive());

    const std::size_t asked = askedAt.size();
    runTicks(engine, 30);
    EXPECT_EQ(askedAt.size(), asked);
}

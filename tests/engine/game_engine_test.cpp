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

// Moves are immediate: walks one cell towards direction and waits until
// the player can act again.
void walk(GameEngine& engine, PlayerId id, Direction direction)
{
    const Position from = engine.getCharacter(id).getPosition();
    act(engine, id, CharacterCommand::move(direction));
    ASSERT_EQ(engine.getCharacter(id).getPosition(), Bomberman::step(from, direction));
    runTicks(engine, engine.getCooldownTicks(id));
    ASSERT_TRUE(engine.isReady(id));
}

// Turns the player towards a blocked cell (e.g. a bomb to push: a move
// towards a free cell would step there) and waits until it can act again.
void face(GameEngine& engine, PlayerId id, Direction direction)
{
    const Position from = engine.getCharacter(id).getPosition();
    act(engine, id, CharacterCommand::move(direction));
    ASSERT_EQ(engine.getCharacter(id).getFacing(), direction);
    ASSERT_EQ(engine.getCharacter(id).getPosition(), from);
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

TEST(GameEngineTest, TurningTowardsABlockedCellTakesTurnTicks)
{
    const GameRules rules;
    Grid grid = openGrid();
    grid.setTile({2, 1}, Tile(TileType::Wall));
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();

    act(engine, id, CharacterCommand::move(Direction::Right));
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 1})); // only turned
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Right);
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
    act(engine, id, CharacterCommand::move(Direction::Right)); // to (2, 1)

    // Submitted while still walking: lost, not replayed later.
    act(engine, id, CharacterCommand::move(Direction::Right));
    runTicks(engine, rules.moveTicks);

    EXPECT_TRUE(engine.isReady(id));
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{2, 1}));
}

TEST(GameEngineTest, MovingOneCellTakesMoveTicks)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer(); // facing down: moving right needs no turn first

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
    GameRules rules;
    rules.startingStats.maxBombs = 0;
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();
    face(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::move(Direction::Right)); // blocked by the wall, already facing it
    EXPECT_TRUE(engine.isReady(id));

    act(engine, id, CharacterCommand::placeBomb()); // no bomb allowed
    EXPECT_TRUE(engine.isReady(id));

    act(engine, id, CharacterCommand::pushBomb()); // no bomb to push
    EXPECT_TRUE(engine.isReady(id));
    EXPECT_EQ(engine.takeEvents().size(), 1u); // only the turn
}

// =========================
// Bombs
// =========================
TEST(GameEngineTest, ABombIsDroppedUnderThePlayerThenThePlayerPauses)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer(); // (1, 1), facing down
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::placeBomb());

    ASSERT_EQ(engine.getBombManager().getBombs().size(), 1u);
    const Bomb& bomb = engine.getBombManager().getBombs().front();
    EXPECT_EQ(bomb.getPosition(), (Position{1, 1}));
    EXPECT_EQ(bomb.getOwnerId(), id);
    EXPECT_FALSE(engine.isReady(id));
    EXPECT_EQ(engine.getCooldownTicks(id), rules.placeBombTicks - 1);

    const std::vector<GameEvent> events = engine.takeEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, GameEvent::Type::BombPlaced);
    EXPECT_EQ(events[0].playerId, id);
    EXPECT_EQ(events[0].position, (Position{1, 1}));
    EXPECT_EQ(events[0].tick, 0u);
}

TEST(GameEngineTest, OneActiveBombPerPlayerByDefault)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb());
    runTicks(engine, rules.placeBombTicks);
    walk(engine, id, Direction::Right);

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
    walk(engine, id, Direction::Right);

    act(engine, id, CharacterCommand::placeBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().size(), 2u);
}

TEST(GameEngineTest, APlayerCanLeaveItsBombButNotStepBackOnIt)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), under the player
    runTicks(engine, rules.placeBombTicks);

    walk(engine, id, Direction::Down); // off the bomb
    act(engine, id, CharacterCommand::move(Direction::Up));

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 2}));
    EXPECT_EQ(engine.getCharacter(id).getFacing(), Direction::Up); // facing the bomb, to push it
}

TEST(GameEngineTest, PushingABombMovesItAndPausesThePlayer)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1)
    runTicks(engine, rules.placeBombTicks);
    walk(engine, id, Direction::Down);
    face(engine, id, Direction::Up); // the bomb blocks the way: only turns
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::pushBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().front().getPosition(), (Position{1, 0}));
    EXPECT_EQ(engine.getCooldownTicks(id), rules.pushBombTicks - 1);
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* pushed = findEvent(events, GameEvent::Type::BombPushed);
    ASSERT_NE(pushed, nullptr);
    EXPECT_EQ(pushed->position, (Position{1, 0}));
}

TEST(GameEngineTest, ABombExplodesAfterExactlyItsFuseInTicks)
{
    Grid grid = openGrid();
    grid.setTile({1, 3}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), placed during this tick
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
    EXPECT_NE(std::find(explosion->cells.begin(), explosion->cells.end(), Position{1, 1}), explosion->cells.end());
}

TEST(GameEngineTest, ExplosionsDestroyBlocksInTheEngineGrid)
{
    Grid grid = openGrid();
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), next to the block

    runTicks(engine, FuseTicks);

    EXPECT_TRUE(engine.getGrid().isWalkable({1, 2}));
}

TEST(GameEngineTest, PlayersInTheBlastAreKilledAndStopActing)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), under the player
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
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1)
    runTicks(engine, rules.placeBombTicks);

    // Run away: off the bomb and down twice, out of the cross of (1, 1).
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
    EXPECT_EQ(engine.getCharacter(second).getPosition(), (Position{5, 4}));
    EXPECT_EQ(engine.getCooldownTicks(first), rules.placeBombTicks - 1);
    EXPECT_EQ(engine.getCooldownTicks(second), rules.moveTicks - 1);

    // The second player's own cooldown is shorter: it can act while the
    // first one is still busy.
    runTicks(engine, rules.moveTicks - 1);
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
    walk(engine, first, Direction::Right);
    act(engine, first, CharacterCommand::placeBomb()); // on (2, 1), next to the second one

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
    // command is submitted every frame. The character starts a 10-tick walk
    // on ticks 0, 10 and 20.
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
    act(engine, id, CharacterCommand::move(Direction::Right)); // walk on tick 0
    runTicks(engine, rules.moveTicks - 2);                     // ticks 1..8, ready on tick 10

    // One slow frame covering ticks 9 and 10: the player becomes ready
    // during it and still gets the command.
    engine.submitCommand(id, CharacterCommand::move(Direction::Right));
    ASSERT_EQ(engine.advance(2.f / 60.f), 2);

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{3, 1}));
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
    EXPECT_EQ(events[0].type, GameEvent::Type::PlayerMoved);
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

// Walks one cell right, from (1, 1) to (2, 1).
void walkRight(GameEngine& engine, PlayerId id)
{
    walk(engine, id, Direction::Right);
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
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    GameEngine engine(std::move(grid), alwaysDrop(UpgradeType::BlastRadius));
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), next to the block
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks);

    // The explosion that revealed it spares it.
    ASSERT_EQ(engine.getUpgrades().size(), 1u);
    EXPECT_EQ(engine.getUpgrades().front(), (Upgrade{{1, 2}, UpgradeType::BlastRadius}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* spawned = findEvent(events, GameEvent::Type::UpgradeSpawned);
    ASSERT_NE(spawned, nullptr);
    EXPECT_EQ(spawned->position, (Position{1, 2}));
    EXPECT_EQ(spawned->upgrade, UpgradeType::BlastRadius);
    EXPECT_EQ(countEvents(events, GameEvent::Type::UpgradeDestroyed), 0);
}

TEST(GameEngineUpgradeTest, NoUpgradeWhenTheDropChanceIsZero)
{
    Grid grid = openGrid();
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    GameRules rules;
    rules.upgrades.dropChancePercent = 0;
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb());

    runTicks(engine, FuseTicks);

    EXPECT_TRUE(engine.getGrid().isWalkable({1, 2}));
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
    walkRight(engine, id); // now on (2, 1)

    act(engine, id, CharacterCommand::placeBomb()); // on (2, 1)
    runTicks(engine, rules.placeBombTicks);
    walk(engine, id, Direction::Down);
    act(engine, id, CharacterCommand::placeBomb()); // on (2, 2)
    runTicks(engine, rules.placeBombTicks);
    walk(engine, id, Direction::Left);
    act(engine, id, CharacterCommand::placeBomb()); // third one, on (1, 2): refused

    EXPECT_EQ(engine.getBombManager().countActiveBombs(id), 2);
}

TEST(GameEngineUpgradeTest, BlastRadiusMakesTheNextBombsReachFurther)
{
    Grid grid = openGrid();
    grid.setTile({4, 1}, Tile(TileType::DestructibleBlock)); // 2 cells from the bomb
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::BlastRadius}));
    walkRight(engine, id);
    ASSERT_EQ(engine.getStats(id).blastRadius, 2);

    act(engine, id, CharacterCommand::placeBomb()); // on (2, 1)
    EXPECT_EQ(engine.getBombManager().getBombs().front().getBlastRadius(), 2);

    runTicks(engine, FuseTicks);
    EXPECT_TRUE(engine.getGrid().isWalkable({4, 1}));
}

TEST(GameEngineUpgradeTest, PushStrengthMakesBombsSlideFurther)
{
    const GameRules rules;
    GameEngine engine(openGrid(), rules);
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{2, 1}, UpgradeType::PushStrength}));
    walkRight(engine, id);
    ASSERT_EQ(engine.getStats(id).pushDistance, 2);
    act(engine, id, CharacterCommand::placeBomb()); // on (2, 1)
    runTicks(engine, rules.placeBombTicks);
    walk(engine, id, Direction::Left); // back to (1, 1), facing left
    face(engine, id, Direction::Right); // the bomb blocks the way: only turns
    (void)engine.takeEvents();

    act(engine, id, CharacterCommand::pushBomb());

    EXPECT_EQ(engine.getBombManager().getBombs().front().getPosition(), (Position{4, 1}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* pushed = findEvent(events, GameEvent::Type::BombPushed);
    ASSERT_NE(pushed, nullptr);
    EXPECT_EQ(pushed->position, (Position{4, 1}));
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
    walk(engine, pusher, Direction::Right);
    act(engine, pusher, CharacterCommand::placeBomb()); // on (2, 1)
    runTicks(engine, rules.placeBombTicks);
    walk(engine, pusher, Direction::Left);
    face(engine, pusher, Direction::Right);

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
    ASSERT_TRUE(engine.addUpgrade({{1, 2}, UpgradeType::PushStrength})); // in the blast of (1, 1)
    ASSERT_TRUE(engine.addUpgrade({{4, 4}, UpgradeType::ExtraBomb}));    // out of it
    act(engine, id, CharacterCommand::placeBomb());                      // on (1, 1)
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks);

    ASSERT_EQ(engine.getUpgrades().size(), 1u);
    EXPECT_EQ(engine.getUpgrades().front().position, (Position{4, 4}));
    const std::vector<GameEvent> events = engine.takeEvents();
    const GameEvent* destroyed = findEvent(events, GameEvent::Type::UpgradeDestroyed);
    ASSERT_NE(destroyed, nullptr);
    EXPECT_EQ(destroyed->position, (Position{1, 2}));
    EXPECT_EQ(destroyed->upgrade, UpgradeType::PushStrength);
}

TEST(GameEngineUpgradeTest, UpgradesDoNotBlockTheWay)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade({{1, 2}, UpgradeType::ExtraBomb}));

    act(engine, id, CharacterCommand::move(Direction::Down));

    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 2}));
}

TEST(GameEngineUpgradeTest, DeadPlayersDoNotCollect)
{
    GameEngine engine(openGrid());
    const PlayerId id = *engine.addPlayer();
    act(engine, id, CharacterCommand::placeBomb()); // on (1, 1), kills the player standing there
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
    grid.addSpawnPoint({3, 3});
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
    engine.submitCommand(first, CharacterCommand::placeBomb());  // on (1, 1)
    engine.submitCommand(second, CharacterCommand::placeBomb()); // on (7, 7)
    engine.tick();
    (void)engine.takeEvents();

    runTicks(engine, FuseTicks - 1);

    std::vector<GameEvent> explosions;
    for (const GameEvent& event : engine.takeEvents())
        if (event.type == GameEvent::Type::Explosion)
            explosions.push_back(event);
    ASSERT_EQ(explosions.size(), 2u);

    const GameEvent& mine = explosions[0];
    EXPECT_EQ(mine.position, (Position{1, 1}));
    EXPECT_EQ(mine.blast.center, (Position{1, 1}));
    EXPECT_EQ(mine.blast.reachTowards(Direction::Down), 3); // hits the block on (1, 4)
    EXPECT_EQ(mine.blast.reachTowards(Direction::Up), 1);   // up to the grid edge
    EXPECT_EQ(mine.cells, mine.blast.cells());

    EXPECT_EQ(explosions[1].blast.center, (Position{7, 7}));
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
    // Walk right twice (10 ticks each), then idle.
    engine.setController(id, std::make_unique<ScriptedController>(
                                 std::vector<CharacterCommand>{CharacterCommand::move(Direction::Right),
                                                               CharacterCommand::move(Direction::Right)},
                                 &askedAt));
    ASSERT_TRUE(engine.hasController(id));

    runTicks(engine, 25);

    // Asked at tick 0 (walk), 10 (walk), then every tick from 20 on.
    const std::vector<std::uint64_t> expected = {0, 10, 20, 21, 22, 23, 24};
    EXPECT_EQ(askedAt, expected);
    EXPECT_EQ(engine.getCharacter(id).getPosition(), (Position{3, 1}));
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
    EXPECT_TRUE(engine.getBombManager().hasBombAt({5, 5})); // the bot dropped it where it stands
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
    for (int i = 0; i < 4; ++i)
        walk(engine, bomber, Direction::Up); // to (5, 1)
    for (int i = 0; i < 3; ++i)
        walk(engine, bomber, Direction::Left); // to (2, 1), next to (1, 1)
    act(engine, bomber, CharacterCommand::placeBomb());
    runTicks(engine, FuseTicks);
    ASSERT_FALSE(engine.getCharacter(id).isAlive());

    const std::size_t asked = askedAt.size();
    runTicks(engine, 30);
    EXPECT_EQ(askedAt.size(), asked);
}

// =========================
// Command recording and state hash (network replay, see doc/network.md)
// =========================
namespace
{

// A crowded board: blocks to blow up so upgrades drop and the random
// generator is used.
Grid crowdedGrid()
{
    Grid grid(9, 9);
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 9; ++x)
        {
            const bool border = x == 0 || y == 0 || x == 8 || y == 8;
            if (border || (x % 2 == 0 && y % 2 == 0))
                grid.setTile({x, y}, Tile(TileType::Wall));
            else if ((x + 2 * y) % 3 == 0)
                grid.setTile({x, y}, Tile(TileType::DestructibleBlock));
        }
    for (const Position spawn : {Position{1, 1}, Position{7, 7}, Position{7, 1}, Position{1, 7}})
    {
        grid.setTile(spawn, Tile(TileType::Empty));
        grid.addSpawnPoint(spawn);
    }
    return grid;
}

// Pseudo-random but reproducible commands for every player, every tick.
CharacterCommand scriptedCommand(Bomberman::Random& random)
{
    switch (random.below(6))
    {
    case 0:
        return CharacterCommand::placeBomb();
    case 1:
        return CharacterCommand::pushBomb();
    default:
        return CharacterCommand::move(static_cast<Direction>(random.below(4)));
    }
}

GameRules dropRules()
{
    GameRules rules;
    rules.seed = 42;
    rules.upgrades.dropChancePercent = 60;
    return rules;
}

} // namespace

TEST(GameEngineReplayTest, RecordingIsOffByDefault)
{
    GameEngine engine(openGrid());
    (void)engine.addPlayer();
    act(engine, 0, CharacterCommand::move(Direction::Right));
    EXPECT_TRUE(engine.takeExecutedCommands().empty());
}

TEST(GameEngineReplayTest, RecordsTheCommandsHandedToReadyPlayersWithTheirTick)
{
    GameEngine engine(openGrid());
    (void)engine.addPlayer();
    (void)engine.addPlayer();
    engine.setCommandRecording(true);

    engine.submitCommand(1, CharacterCommand::move(Direction::Up));
    act(engine, 0, CharacterCommand::move(Direction::Right)); // tick 0: both act
    act(engine, 0, CharacterCommand::placeBomb());            // tick 1: player 0 is walking: dropped

    const std::vector<Bomberman::ExecutedCommand> recorded = engine.takeExecutedCommands();
    ASSERT_EQ(recorded.size(), 2u);
    EXPECT_EQ(recorded[0], (Bomberman::ExecutedCommand{0, 0, CharacterCommand::move(Direction::Right)}));
    EXPECT_EQ(recorded[1], (Bomberman::ExecutedCommand{0, 1, CharacterCommand::move(Direction::Up)}));
    EXPECT_TRUE(engine.takeExecutedCommands().empty()); // taken
}

TEST(GameEngineReplayTest, CommandsWithoutEffectAreRecordedToo)
{
    Grid grid = openGrid();
    grid.setTile({1, 0}, Tile(TileType::Wall));
    GameEngine engine(std::move(grid));
    (void)engine.addPlayer();
    engine.setCommandRecording(true);

    act(engine, 0, CharacterCommand::pushBomb()); // no bomb in front: nothing happens
    EXPECT_EQ(engine.takeExecutedCommands().size(), 1u);
}

TEST(GameEngineReplayTest, ReplayingTheRecordedCommandsReproducesTheMatch)
{
    GameEngine host(crowdedGrid(), dropRules());
    GameEngine client(crowdedGrid(), dropRules());
    for (int i = 0; i < 4; ++i)
    {
        (void)host.addPlayer();
        (void)client.addPlayer();
    }
    host.setCommandRecording(true);

    Bomberman::Random script(7);
    int upgradesSeen = 0;
    for (int tick = 0; tick < 1200; ++tick)
    {
        for (PlayerId id = 0; id < 4; ++id)
            host.submitCommand(id, scriptedCommand(script));
        host.tick();

        for (const Bomberman::ExecutedCommand& executed : host.takeExecutedCommands())
        {
            ASSERT_EQ(executed.tick, client.getTick());
            client.submitCommand(executed.player, executed.command);
        }
        client.tick();

        ASSERT_EQ(client.stateHash(), host.stateHash()) << "tick " << tick;
        const std::vector<GameEvent> hostEvents = host.takeEvents();
        const std::vector<GameEvent> clientEvents = client.takeEvents();
        ASSERT_EQ(clientEvents.size(), hostEvents.size()) << "tick " << tick;
        upgradesSeen += countEvents(hostEvents, GameEvent::Type::UpgradeSpawned);
    }
    EXPECT_GT(upgradesSeen, 0); // the random generator really was exercised
}

TEST(GameEngineReplayTest, TheStateHashTellsMatchesApart)
{
    GameEngine a(openGrid());
    GameEngine b(openGrid());
    (void)a.addPlayer();
    (void)b.addPlayer();
    EXPECT_EQ(a.stateHash(), b.stateHash());

    a.tick();
    EXPECT_NE(a.stateHash(), b.stateHash()); // tick count
    b.tick();
    ASSERT_EQ(a.stateHash(), b.stateHash());

    act(a, 0, CharacterCommand::move(Direction::Right));
    b.tick();
    EXPECT_NE(a.stateHash(), b.stateHash()); // position

    GameRules otherSeed;
    otherSeed.seed = 1;
    EXPECT_NE(GameEngine(openGrid()).stateHash(), GameEngine(openGrid(), otherSeed).stateHash());
}

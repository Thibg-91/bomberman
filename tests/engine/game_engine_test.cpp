#include <algorithm>
#include <stdexcept>
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
    rules.maxActiveBombs = 2;
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

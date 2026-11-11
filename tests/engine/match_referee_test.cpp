#include <vector>

#include <gtest/gtest.h>

#include "game_engine.h"
#include "match_referee.h"

using Bomberman::CharacterCommand;
using Bomberman::GameEngine;
using Bomberman::Grid;
using Bomberman::MatchReferee;
using Bomberman::PlayerId;
using Bomberman::Position;

namespace
{

constexpr int FuseTicks = 120;
using Phase = MatchReferee::Phase;

// Empty 7x7 board with the given spawn points; one player per spawn point.
GameEngine engineWith(const std::vector<Position>& spawns)
{
    Grid grid(7, 7);
    for (const Position spawn : spawns)
        grid.addSpawnPoint(spawn);
    GameEngine engine(std::move(grid));
    for (std::size_t i = 0; i < spawns.size(); ++i)
        (void)engine.addPlayer();
    return engine;
}

// Player id drops a bomb where it stands (and stays there) and the fuse
// burns out; the referee looks after every tick.
void bombHere(GameEngine& engine, MatchReferee& referee, PlayerId id)
{
    engine.submitCommand(id, CharacterCommand::placeBomb());
    for (int i = 0; i <= FuseTicks; ++i)
    {
        engine.tick();
        referee.update(engine, 1.f / 60.f);
    }
}

} // namespace

TEST(MatchRefereeTest, AMatchStartsInPlayAtNormalSpeed)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}});
    MatchReferee referee(0);
    referee.update(engine, 0.1f);

    EXPECT_EQ(referee.getPhase(), Phase::Playing);
    EXPECT_FALSE(referee.isOver());
    EXPECT_FLOAT_EQ(referee.gameSeconds(0.5f), 0.5f);
    EXPECT_EQ(referee.getWinner(), std::nullopt);
    EXPECT_EQ(referee.getHuman(), 0);
}

TEST(MatchRefereeTest, TheLastOneStandingWins)
{
    GameEngine engine = engineWith({{1, 1}, {5, 1}});
    MatchReferee referee(0);
    bombHere(engine, referee, 1); // the opponent blows itself up

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 0);
}

TEST(MatchRefereeTest, AfterThePlayerDiesTheOpponentsFightItOutFaster)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    MatchReferee referee(0);
    bombHere(engine, referee, 0); // the player blows itself up

    ASSERT_EQ(referee.getPhase(), Phase::Spectating);
    EXPECT_FLOAT_EQ(referee.gameSeconds(0.25f), 0.25f * MatchReferee::SpectatorSpeed);

    bombHere(engine, referee, 1); // then the second player
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 2);
}

TEST(MatchRefereeTest, DyingTogetherIsADraw)
{
    GameEngine engine = engineWith({{1, 1}, {1, 2}}); // side by side
    MatchReferee referee(0);
    bombHere(engine, referee, 0);

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), std::nullopt);
}

TEST(MatchRefereeTest, SpectatingForTooLongEndsInADraw)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    MatchReferee referee(0);
    bombHere(engine, referee, 0);
    ASSERT_EQ(referee.getPhase(), Phase::Spectating);

    for (int second = 0; second < static_cast<int>(MatchReferee::SpectatorTimeLimit) - 1; ++second)
        referee.update(engine, 1.f);
    EXPECT_EQ(referee.getPhase(), Phase::Spectating);
    referee.update(engine, 1.f);

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), std::nullopt);
}

TEST(MatchRefereeTest, AloneOnTheMapTheMatchOnlyEndsWithThePlayer)
{
    GameEngine engine = engineWith({{1, 1}});
    MatchReferee referee(0);
    referee.update(engine, 1.f);
    EXPECT_EQ(referee.getPhase(), Phase::Playing);

    bombHere(engine, referee, 0);
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), std::nullopt);
}

TEST(MatchRefereeTest, AnOverMatchIsFrozenAndStaysOver)
{
    GameEngine engine = engineWith({{1, 1}, {5, 1}});
    MatchReferee referee(0);
    bombHere(engine, referee, 1);
    ASSERT_TRUE(referee.isOver());

    EXPECT_FLOAT_EQ(referee.gameSeconds(1.f), 0.f);
    referee.update(engine, 100.f);
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 0);
}

TEST(MatchRefereeTest, InRealTimeTheDeadWatchAtNormalSpeedWithoutTimeLimit)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    MatchReferee referee(0, MatchReferee::SpectatorRule::RealTime);
    EXPECT_EQ(referee.getSpectatorRule(), MatchReferee::SpectatorRule::RealTime);
    bombHere(engine, referee, 0);
    ASSERT_EQ(referee.getPhase(), Phase::Spectating);

    EXPECT_FLOAT_EQ(referee.gameSeconds(0.25f), 0.25f); // the other humans play on
    for (int second = 0; second < 10 * static_cast<int>(MatchReferee::SpectatorTimeLimit); ++second)
        referee.update(engine, 1.f);
    EXPECT_EQ(referee.getPhase(), Phase::Spectating);

    bombHere(engine, referee, 1);
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 2);
}

namespace
{

// Three players far apart; player 0 drops a bomb where it stands at tick 0,
// player 1 at tick 5 and player 2 at tick 8: they die at ticks 120, 125 and
// 128. The match is decided at tick 125 (player 2 alone), whatever comes
// after.
GameEngine threeBombers(std::vector<Bomberman::GameEvent>& events, int ticks)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    for (int tick = 0; tick < ticks; ++tick)
    {
        if (tick == 0 || tick == 5 || tick == 8)
            engine.submitCommand(tick == 0 ? 0 : tick == 5 ? 1 : 2, CharacterCommand::placeBomb());
        engine.tick();
        for (Bomberman::GameEvent& event : engine.takeEvents())
            events.push_back(std::move(event));
    }
    return engine;
}

} // namespace

TEST(MatchRefereeTest, TheFirstDecisiveTickDecidesEvenWithinOneFrame)
{
    std::vector<Bomberman::GameEvent> events;
    const GameEngine engine = threeBombers(events, 130); // one long frame
    ASSERT_EQ(engine.countAlivePlayers(), 0);

    MatchReferee referee(0, MatchReferee::SpectatorRule::RealTime);
    referee.update(engine, 130.f / 60.f, events);

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 2); // alone at tick 125, dead at 128 does not matter
}

TEST(MatchRefereeTest, TheVerdictDoesNotDependOnHowTicksAreGroupedInFrames)
{
    // Host-like frames of 7 ticks vs client-like frames of 1 tick.
    for (const int frame : {1, 3, 7, 130})
    {
        GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
        MatchReferee referee(1, MatchReferee::SpectatorRule::RealTime);
        std::vector<Bomberman::GameEvent> frameEvents;
        for (int tick = 0; tick < 130 && !referee.isOver(); ++tick)
        {
            if (tick == 0 || tick == 5 || tick == 8)
                engine.submitCommand(tick == 0 ? 0 : tick == 5 ? 1 : 2, CharacterCommand::placeBomb());
            engine.tick();
            for (Bomberman::GameEvent& event : engine.takeEvents())
                frameEvents.push_back(std::move(event));
            if ((tick + 1) % frame == 0 || tick == 129)
            {
                referee.update(engine, static_cast<float>(frame) / 60.f, frameEvents);
                frameEvents.clear();
            }
        }
        EXPECT_TRUE(referee.isOver()) << "frames of " << frame;
        EXPECT_EQ(referee.getWinner(), 2) << "frames of " << frame;
    }
}

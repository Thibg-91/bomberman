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

// Player id drops a bomb on the cell below it (players face down at
// first) and the fuse burns out; the referee looks after every tick.
void bombBelow(GameEngine& engine, MatchReferee& referee, PlayerId id)
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
    bombBelow(engine, referee, 1); // the opponent blows itself up

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 0);
}

TEST(MatchRefereeTest, AfterThePlayerDiesTheOpponentsFightItOutFaster)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    MatchReferee referee(0);
    bombBelow(engine, referee, 0); // the player blows itself up

    ASSERT_EQ(referee.getPhase(), Phase::Spectating);
    EXPECT_FLOAT_EQ(referee.gameSeconds(0.25f), 0.25f * MatchReferee::SpectatorSpeed);

    bombBelow(engine, referee, 1); // then the second player
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 2);
}

TEST(MatchRefereeTest, DyingTogetherIsADraw)
{
    GameEngine engine = engineWith({{1, 1}, {1, 3}}); // both next to (1, 2)
    MatchReferee referee(0);
    bombBelow(engine, referee, 0);

    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), std::nullopt);
}

TEST(MatchRefereeTest, SpectatingForTooLongEndsInADraw)
{
    GameEngine engine = engineWith({{1, 1}, {5, 5}, {5, 1}});
    MatchReferee referee(0);
    bombBelow(engine, referee, 0);
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

    bombBelow(engine, referee, 0);
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), std::nullopt);
}

TEST(MatchRefereeTest, AnOverMatchIsFrozenAndStaysOver)
{
    GameEngine engine = engineWith({{1, 1}, {5, 1}});
    MatchReferee referee(0);
    bombBelow(engine, referee, 1);
    ASSERT_TRUE(referee.isOver());

    EXPECT_FLOAT_EQ(referee.gameSeconds(1.f), 0.f);
    referee.update(engine, 100.f);
    EXPECT_TRUE(referee.isOver());
    EXPECT_EQ(referee.getWinner(), 0);
}

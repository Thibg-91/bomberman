#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "bot.h"
#include "grid_loader.h"

using Bomberman::Bot;
using Bomberman::BotDifficulty;
using Bomberman::GameEngine;
using Bomberman::GameEvent;
using Bomberman::GameRules;
using Bomberman::PlayerId;

namespace
{

constexpr int TicksPerSecond = 60;

GameEngine shippedMatch(int level, std::uint64_t seed)
{
    GameRules rules;
    rules.seed = seed;
    return GameEngine(Bomberman::GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_0"
                                                          + std::to_string(level) + ".json"),
                      rules);
}

// One bot per spawn point, difficulties in turn.
void fillWithBots(GameEngine& engine, const std::vector<BotDifficulty>& difficulties, std::uint64_t seed)
{
    for (const BotDifficulty difficulty : difficulties)
    {
        const PlayerId id = *engine.addPlayer();
        engine.setController(id, std::make_unique<Bot>(difficulty, seed + static_cast<std::uint64_t>(id)));
    }
}

// What happened, in a comparable form.
using Trace = std::vector<std::tuple<int, std::uint64_t, PlayerId, int, int>>;

void record(GameEngine& engine, Trace& trace)
{
    for (const GameEvent& event : engine.takeEvents())
        trace.emplace_back(static_cast<int>(event.type), event.tick, event.playerId, event.position.x,
                           event.position.y);
}

const std::vector<BotDifficulty> Mixed = {BotDifficulty::Easy, BotDifficulty::Normal, BotDifficulty::Hard,
                                          BotDifficulty::Normal};

} // namespace

// Bots draw their randomness from their own seeded generator: the same
// seeds replay the same match, event for event.
TEST(BotMatchTest, TheSameSeedsPlayTheSameMatch)
{
    Trace first;
    Trace second;
    for (Trace* trace : {&first, &second})
    {
        GameEngine engine = shippedMatch(1, 42);
        fillWithBots(engine, Mixed, 5);
        for (int i = 0; i < 40 * TicksPerSecond; ++i)
        {
            engine.tick();
            record(engine, *trace);
        }
    }

    EXPECT_GT(first.size(), 50u); // something happened
    EXPECT_EQ(first, second);
}

TEST(BotMatchTest, OtherSeedsPlayAnotherMatch)
{
    Trace first;
    Trace second;
    for (const std::uint64_t seed : {5u, 6u})
    {
        GameEngine engine = shippedMatch(1, 42);
        fillWithBots(engine, Mixed, seed);
        for (int i = 0; i < 40 * TicksPerSecond; ++i)
        {
            engine.tick();
            record(engine, seed == 5u ? first : second);
        }
    }
    EXPECT_NE(first, second);
}

// Bots are asked by the engine at each tick, not once per frame: the frame
// rate changes nothing.
class BotFrameRateTest : public ::testing::TestWithParam<int>
{
};

INSTANTIATE_TEST_SUITE_P(FrameRates, BotFrameRateTest, ::testing::Values(30, 144));

TEST_P(BotFrameRateTest, BotsPlayTheSameWhateverTheFrameRate)
{
    const std::uint64_t ticks = 30 * TicksPerSecond;

    Trace reference;
    {
        GameEngine engine = shippedMatch(2, 7);
        fillWithBots(engine, Mixed, 9);
        while (engine.getTick() < ticks)
        {
            engine.tick();
            record(engine, reference);
        }
    }

    Trace framed;
    GameEngine engine = shippedMatch(2, 7);
    fillWithBots(engine, Mixed, 9);
    const float frame = 1.f / static_cast<float>(GetParam());
    while (engine.getTick() < ticks)
    {
        engine.advance(frame);
        record(engine, framed);
    }
    // advance() may overshoot by a tick at 30 fps.
    while (!framed.empty() && std::get<1>(framed.back()) >= ticks)
        framed.pop_back();

    EXPECT_EQ(framed, reference);
}

// Duels on every shipped map, both spawn orders, four seeds (most duels
// between bots end in a draw after 3 minutes: they dodge well): the hard
// bot must win clearly more often than the easy one.
TEST(BotMatchTest, HardBotsWinMoreDuelsThanEasyOnes)
{
    int hardWins = 0;
    int easyWins = 0;
    for (int level = 1; level <= 4; ++level)
        for (int seed = 0; seed < 4; ++seed)
        for (int hardFirst = 0; hardFirst < 2; ++hardFirst)
        {
            GameEngine engine = shippedMatch(level, 900 + static_cast<std::uint64_t>(seed));
            const PlayerId hard = hardFirst ? 0 : 1;
            fillWithBots(engine,
                         hardFirst ? std::vector<BotDifficulty>{BotDifficulty::Hard, BotDifficulty::Easy}
                                   : std::vector<BotDifficulty>{BotDifficulty::Easy, BotDifficulty::Hard},
                         13 * static_cast<std::uint64_t>(seed));
            for (int i = 0; i < 180 * TicksPerSecond && engine.countAlivePlayers() > 1; ++i)
                engine.tick();

            const bool hardAlive = engine.getCharacter(hard).isAlive();
            const bool easyAlive = engine.getCharacter(1 - hard).isAlive();
            hardWins += hardAlive && !easyAlive ? 1 : 0;
            easyWins += easyAlive && !hardAlive ? 1 : 0;
        }

    EXPECT_GT(hardWins, easyWins);
    EXPECT_GE(hardWins, 4);
}

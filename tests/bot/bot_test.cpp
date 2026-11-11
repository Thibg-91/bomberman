#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "bot.h"
#include "grid_loader.h"

using Bomberman::AllBotDifficulties;
using Bomberman::Bot;
using Bomberman::BotDifficulty;
using Bomberman::BotProfile;
using Bomberman::CharacterCommand;
using Bomberman::DangerMap;
using Bomberman::Direction;
using Bomberman::GameEngine;
using Bomberman::GameEvent;
using Bomberman::GameRules;
using Bomberman::Grid;
using Bomberman::PlayerId;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;
using Bomberman::Upgrade;
using Bomberman::UpgradeType;

namespace
{

constexpr int TicksPerSecond = 60;

// A grid made of walls except for the '.' (empty) and '+' (block) cells;
// 'P' marks spawn points (empty), in reading order.
Grid fromRows(const std::vector<std::string>& rows)
{
    Grid grid(static_cast<int>(rows.front().size()), static_cast<int>(rows.size()), TileType::Wall);
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
        {
            const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (c == '.' || c == 'P')
                grid.setTile({x, y}, Tile(TileType::Empty));
            else if (c == '+')
                grid.setTile({x, y}, Tile(TileType::DestructibleBlock));
            if (c == 'P')
                grid.addSpawnPoint({x, y});
        }
    return grid;
}

Grid shippedLevel(int index)
{
    return Bomberman::GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_0"
                                               + std::to_string(index) + ".json");
}

// Hands player id over to a bot and returns it (the engine owns it).
Bot* control(GameEngine& engine, PlayerId id, const BotProfile& profile, std::uint64_t seed = 1)
{
    auto bot = std::make_unique<Bot>(profile, seed);
    Bot* raw = bot.get();
    engine.setController(id, std::move(bot));
    return raw;
}

Bot* control(GameEngine& engine, PlayerId id, BotDifficulty difficulty, std::uint64_t seed = 1)
{
    return control(engine, id, BotProfile::forDifficulty(difficulty), seed);
}

// Runs ticks and returns every event.
std::vector<GameEvent> run(GameEngine& engine, int ticks)
{
    std::vector<GameEvent> events;
    for (int i = 0; i < ticks; ++i)
    {
        engine.tick();
        for (GameEvent& event : engine.takeEvents())
            events.push_back(std::move(event));
    }
    return events;
}

int count(const std::vector<GameEvent>& events, GameEvent::Type type, PlayerId id = -2)
{
    int n = 0;
    for (const GameEvent& event : events)
        if (event.type == type && (id == -2 || event.playerId == id))
            ++n;
    return n;
}

int countBlocks(const Grid& grid)
{
    int n = 0;
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
            n += grid.getTile({x, y}).isDestructible() ? 1 : 0;
    return n;
}

// A profile that does nothing but what a test enables.
BotProfile quiet()
{
    BotProfile profile;
    profile.blockValue = 0;
    return profile;
}

std::string name(BotDifficulty difficulty)
{
    switch (difficulty)
    {
    case BotDifficulty::Easy:
        return "Easy";
    case BotDifficulty::Normal:
        return "Normal";
    case BotDifficulty::Hard:
        return "Hard";
    }
    return "?";
}

} // namespace

// =========================
// Profiles
// =========================
TEST(BotProfileTest, TheMenuListsTheLevelsFromEasyToHard)
{
    EXPECT_EQ(AllBotDifficulties.size(), 3u);
    EXPECT_EQ(AllBotDifficulties[0], BotDifficulty::Easy);
    EXPECT_EQ(AllBotDifficulties[1], BotDifficulty::Normal);
    EXPECT_EQ(AllBotDifficulties[2], BotDifficulty::Hard);
}

TEST(BotProfileTest, HarderLevelsReactFasterHesitateAndErrLess)
{
    const BotProfile easy = BotProfile::forDifficulty(BotDifficulty::Easy);
    const BotProfile normal = BotProfile::forDifficulty(BotDifficulty::Normal);
    const BotProfile hard = BotProfile::forDifficulty(BotDifficulty::Hard);

    EXPECT_GT(easy.reactionTicks, normal.reactionTicks);
    EXPECT_GT(normal.reactionTicks, hard.reactionTicks);
    EXPECT_EQ(hard.reactionTicks, 0);

    EXPECT_GT(easy.hesitationPercent, normal.hesitationPercent);
    EXPECT_GT(normal.hesitationPercent, hard.hesitationPercent);
    EXPECT_EQ(hard.hesitationPercent, 0);

    EXPECT_GT(easy.mistakePercent, normal.mistakePercent);
    EXPECT_GT(normal.mistakePercent, hard.mistakePercent);
    EXPECT_EQ(hard.mistakePercent, 0);
}

TEST(BotProfileTest, HarderLevelsUseMoreBombsAndTricks)
{
    const BotProfile easy = BotProfile::forDifficulty(BotDifficulty::Easy);
    const BotProfile normal = BotProfile::forDifficulty(BotDifficulty::Normal);
    const BotProfile hard = BotProfile::forDifficulty(BotDifficulty::Hard);

    EXPECT_LT(easy.maxBombs, normal.maxBombs);
    EXPECT_LT(normal.maxBombs, hard.maxBombs);
    EXPECT_LT(easy.upgradeValue, normal.upgradeValue);
    EXPECT_LT(normal.upgradeValue, hard.upgradeValue);
    EXPECT_LT(easy.attackValue, normal.attackValue);

    EXPECT_FALSE(easy.huntsOpponents);
    EXPECT_TRUE(normal.huntsOpponents);
    EXPECT_TRUE(hard.huntsOpponents);
    EXPECT_FALSE(normal.pushesBombs);
    EXPECT_TRUE(hard.pushesBombs);
    EXPECT_EQ(normal.trapValue, 0);
    EXPECT_GT(hard.trapValue, 0);
}

// =========================
// Perception
// =========================
TEST(BotTest, OwnBombsAreKnownAtOnceTheOthersAfterTheReactionTime)
{
    Grid grid(9, 9);
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({7, 7});
    GameEngine engine(std::move(grid));
    const PlayerId me = *engine.addPlayer();
    const PlayerId other = *engine.addPlayer();
    engine.submitCommand(me, CharacterCommand::placeBomb());    // on (1, 1)
    engine.submitCommand(other, CharacterCommand::placeBomb()); // on (7, 7)
    engine.tick(); // both bombs have been ticking for 1 tick

    const int reaction = 30;
    DangerMap danger = Bot::perceive(engine, me, reaction, engine.getBombManager());
    EXPECT_TRUE(danger.isThreatened({1, 2}));
    EXPECT_FALSE(danger.isThreatened({7, 6}));
    EXPECT_EQ(danger.freeFrom({7, 7}), DangerMap::Forever); // still blocks the way

    for (int i = 0; i < reaction - 1; ++i)
        engine.tick();
    danger = Bot::perceive(engine, me, reaction, engine.getBombManager());
    EXPECT_TRUE(danger.isThreatened({7, 6}));
    EXPECT_TRUE(Bot::perceive(engine, other, 0, engine.getBombManager()).isThreatened({1, 2}));
}

// =========================
// Survival
// =========================
class BotDifficultyTest : public ::testing::TestWithParam<BotDifficulty>
{
};

INSTANTIATE_TEST_SUITE_P(AllDifficulties, BotDifficultyTest, ::testing::ValuesIn(AllBotDifficulties),
                         [](const auto& info) { return name(info.param); });

TEST_P(BotDifficultyTest, ABotGetsOutOfTheWayOfABomb)
{
    GameEngine engine(fromRows({
        "#######",
        "#..P..#",
        "#..P..#",
        "#.....#",
        "#######",
    }));
    const PlayerId human = *engine.addPlayer(); // (3, 1)
    const PlayerId bot = *engine.addPlayer();   // (3, 2), right under
    engine.submitCommand(human, CharacterCommand::placeBomb()); // on (3, 1): the bot is in the blast
    engine.tick();
    Bot* brain = control(engine, bot, GetParam());

    bool sawEscape = false;
    std::vector<GameEvent> events;
    for (int i = 0; i < 3 * TicksPerSecond; ++i)
    {
        const std::vector<GameEvent> tick = run(engine, 1);
        events.insert(events.end(), tick.begin(), tick.end());
        sawEscape = sawEscape || brain->getIntent() == Bot::Intent::Escape;
    }

    EXPECT_GE(count(events, GameEvent::Type::Explosion), 1);
    EXPECT_TRUE(sawEscape);
    EXPECT_TRUE(engine.getCharacter(bot).isAlive());
}

TEST_P(BotDifficultyTest, ItNeverDropsABombItCouldNotEscape)
{
    // A dead end: a bomb on (2, 1) would break the block but leave no way
    // out (the bomb blocks (2, 1), (1, 1) is in its blast).
    GameEngine engine(fromRows({
        "#####",
        "#P.+#",
        "#####",
    }));
    const PlayerId id = *engine.addPlayer();
    control(engine, id, GetParam());

    const std::vector<GameEvent> events = run(engine, 5 * TicksPerSecond);

    EXPECT_EQ(count(events, GameEvent::Type::BombPlaced), 0);
    EXPECT_TRUE(engine.getCharacter(id).isAlive());
}

// Alone on each shipped map for a minute: the bot clears blocks and never
// blows itself up.
class BotLevelTest : public ::testing::TestWithParam<std::tuple<int, BotDifficulty>>
{
};

INSTANTIATE_TEST_SUITE_P(ShippedLevels, BotLevelTest,
                         ::testing::Combine(::testing::Values(1, 2, 3, 4), ::testing::ValuesIn(AllBotDifficulties)),
                         [](const auto& info) {
                             return "Level" + std::to_string(std::get<0>(info.param)) + name(std::get<1>(info.param));
                         });

TEST_P(BotLevelTest, ALoneBotClearsBlocksWithoutBlowingItselfUp)
{
    const auto [level, difficulty] = GetParam();
    GameEngine engine(shippedLevel(level));
    const int blocksBefore = countBlocks(engine.getGrid());
    const PlayerId id = *engine.addPlayer();
    control(engine, id, difficulty, 7);

    const std::vector<GameEvent> events = run(engine, 60 * TicksPerSecond);

    EXPECT_TRUE(engine.getCharacter(id).isAlive());
    EXPECT_GE(count(events, GameEvent::Type::BombPlaced, id), 10);
    EXPECT_LE(countBlocks(engine.getGrid()), blocksBefore - 20);
}

// =========================
// Decisions
// =========================
TEST(BotTest, ItDropsABombNextToABlockWhenItCanGetAway)
{
    GameEngine engine(fromRows({
        "######",
        "#P...#",
        "#+...#",
        "######",
    }));
    const PlayerId id = *engine.addPlayer(); // (1, 1): the bomb goes under it, next to the block
    Bot* bot = control(engine, id, BotDifficulty::Hard);

    const std::vector<GameEvent> events = run(engine, 1);

    ASSERT_EQ(count(events, GameEvent::Type::BombPlaced), 1);
    EXPECT_EQ(events.front().position, (Position{1, 1}));
    EXPECT_EQ(bot->getIntent(), Bot::Intent::PlaceBomb);
}

TEST(BotTest, ItWalksToASpotFromWhichToBreakBlocks)
{
    GameEngine engine(fromRows({
        "#######",
        "#P....#",
        "#####+#",
        "#######",
    }));
    const PlayerId id = *engine.addPlayer();
    Bot* bot = control(engine, id, BotDifficulty::Hard);

    run(engine, 1);
    EXPECT_EQ(bot->getIntent(), Bot::Intent::BreakBlocks);
    // The only spot: on (5, 1), right above the block.
    const std::vector<GameEvent> events = run(engine, 2 * TicksPerSecond);
    ASSERT_GE(count(events, GameEvent::Type::BombPlaced, id), 1);
}

class BotCollectorTest : public ::testing::TestWithParam<BotDifficulty>
{
};

INSTANTIATE_TEST_SUITE_P(NormalAndHard, BotCollectorTest,
                         ::testing::Values(BotDifficulty::Normal, BotDifficulty::Hard),
                         [](const auto& info) { return name(info.param); });

TEST_P(BotCollectorTest, ItGoesForANearbyUpgrade)
{
    Grid grid(7, 7);
    grid.addSpawnPoint({1, 1});
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade(Upgrade{{4, 3}, UpgradeType::BlastRadius}));
    control(engine, id, GetParam());

    const std::vector<GameEvent> events = run(engine, 5 * TicksPerSecond);

    EXPECT_EQ(count(events, GameEvent::Type::UpgradeCollected, id), 1);
    EXPECT_EQ(engine.getStats(id).blastRadius, 2);
}

TEST(BotTest, ItLeavesUpgradesItCannotUseAnyMore)
{
    Grid grid(7, 7);
    grid.addSpawnPoint({1, 1});
    GameRules rules;
    rules.startingStats.blastRadius = rules.upgrades.maxBlastRadius; // already at the cap
    GameEngine engine(std::move(grid), rules);
    const PlayerId id = *engine.addPlayer();
    ASSERT_TRUE(engine.addUpgrade(Upgrade{{4, 3}, UpgradeType::BlastRadius}));
    BotProfile profile = quiet();
    profile.upgradeValue = 50;
    Bot* bot = control(engine, id, profile);

    run(engine, 1);

    EXPECT_NE(bot->getIntent(), Bot::Intent::Collect);
}

class BotHunterTest : public ::testing::TestWithParam<BotDifficulty>
{
};

INSTANTIATE_TEST_SUITE_P(NormalAndHard, BotHunterTest, ::testing::Values(BotDifficulty::Normal, BotDifficulty::Hard),
                         [](const auto& info) { return name(info.param); });

TEST_P(BotHunterTest, ItHuntsDownAnOpponentThatDoesNotMove)
{
    GameEngine engine(fromRows({
        "#########",
        "#P......#",
        "#.#.#.#.#",
        "#.......#",
        "#.#.#.#.#",
        "#.......#",
        "#.#.#.#.#",
        "#......P#",
        "#########",
    }));
    const PlayerId bot = *engine.addPlayer();
    const PlayerId target = *engine.addPlayer();
    control(engine, bot, GetParam(), 3);

    for (int i = 0; i < 40 * TicksPerSecond && engine.getCharacter(target).isAlive(); ++i)
        run(engine, 1);

    EXPECT_FALSE(engine.getCharacter(target).isAlive());
    EXPECT_TRUE(engine.getCharacter(bot).isAlive());
}

// The opponent is stuck at the end of a corridor: a bomb at its entrance
// leaves it no way out, which a trapper sees even without valuing direct
// hits.
TEST(BotTest, ATrapperDropsABombThatLeavesTheOpponentNoWayOut)
{
    const std::vector<std::string> rows = {
        "#######",
        "#P.P..#",
        "####.##",
        "#######",
    };
    GameRules rules;
    rules.startingStats.blastRadius = 2;

    BotProfile trapper = quiet();
    trapper.trapValue = 100;
    for (const bool traps : {true, false})
    {
        GameEngine engine(fromRows(rows), rules);
        const PlayerId opponent = *engine.addPlayer(); // (1, 1), dead end
        const PlayerId id = *engine.addPlayer();       // (3, 1), the corridor's entrance
        BotProfile profile = trapper;
        profile.trapValue = traps ? 100 : 0;
        control(engine, id, profile);

        const std::vector<GameEvent> events = run(engine, 1);

        EXPECT_EQ(count(events, GameEvent::Type::BombPlaced, id), traps ? 1 : 0) << "traps " << traps;
        if (traps)
        {
            run(engine, 3 * TicksPerSecond);
            EXPECT_FALSE(engine.getCharacter(opponent).isAlive());
            EXPECT_TRUE(engine.getCharacter(id).isAlive()); // got away through (4, 1) and (4, 2)
        }
    }
}

// Next to its own bomb (so in its blast), facing it, with an opponent two
// cells further: a hard bot punches the bomb onto the opponent, a normal
// one just walks away.
TEST(BotTest, AHardBotPunchesABombOntoAnOpponent)
{
    const std::vector<std::string> rows = {
        "#######",
        "#P..P.#",
        "#.#####",
        "#######",
    };
    for (const BotDifficulty difficulty : {BotDifficulty::Hard, BotDifficulty::Normal})
    {
        GameEngine engine(fromRows(rows));
        const PlayerId id = *engine.addPlayer();       // (1, 1)
        const PlayerId opponent = *engine.addPlayer(); // (4, 1)
        const auto act = [&](const CharacterCommand& command) {
            engine.submitCommand(id, command);
            engine.tick();
            run(engine, engine.getCooldownTicks(id));
        };
        act(CharacterCommand::move(Direction::Right)); // to (2, 1)
        act(CharacterCommand::placeBomb());            // on (2, 1)
        act(CharacterCommand::move(Direction::Left));  // back to (1, 1)
        act(CharacterCommand::move(Direction::Right)); // the bomb blocks the way: only turns towards it
        ASSERT_EQ(engine.getCharacter(id).getPosition(), (Position{1, 1}));
        Bot* bot = control(engine, id, difficulty);

        const std::vector<GameEvent> events = run(engine, 1);

        if (difficulty == BotDifficulty::Hard)
        {
            ASSERT_EQ(count(events, GameEvent::Type::BombPushed, id), 1);
            EXPECT_EQ(events.front().position, (Position{3, 1})); // stops before the opponent
            EXPECT_EQ(bot->getIntent(), Bot::Intent::PushBomb);
            run(engine, 2 * TicksPerSecond);
            EXPECT_FALSE(engine.getCharacter(opponent).isAlive());
            EXPECT_TRUE(engine.getCharacter(id).isAlive());
        }
        else
        {
            EXPECT_EQ(count(events, GameEvent::Type::BombPushed), 0);
            EXPECT_EQ(bot->getIntent(), Bot::Intent::Escape);
        }
    }
}

TEST(BotTest, ASpotWhereABombWouldBeDeadlyIsLeftAsideForAWhile)
{
    // A bomb on (2, 1) would break the block but there is no way out: the
    // bot does not stay there forever trying.
    GameEngine engine(fromRows({
        "#####",
        "#.P+#",
        "#####",
    }));
    const PlayerId id = *engine.addPlayer();
    Bot* bot = control(engine, id, BotDifficulty::Hard);

    run(engine, 1);

    EXPECT_NE(bot->getIntent(), Bot::Intent::PlaceBomb);
    EXPECT_NE(bot->getIntent(), Bot::Intent::BreakBlocks);
}

TEST(BotTest, EasyBotsSometimesHesitate)
{
    Grid grid(9, 9);
    grid.addSpawnPoint({1, 1});
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    Bot* bot = control(engine, id, BotDifficulty::Easy, 11);

    bool hesitated = false;
    for (int i = 0; i < 10 * TicksPerSecond && !hesitated; ++i)
    {
        run(engine, 1);
        hesitated = bot->getIntent() == Bot::Intent::Hesitate;
    }
    EXPECT_TRUE(hesitated);
}

TEST(BotTest, AnIdleBotHasNoTarget)
{
    Grid grid(3, 3, TileType::Wall);
    grid.setTile({1, 1}, Tile(TileType::Empty));
    grid.addSpawnPoint({1, 1});
    GameEngine engine(std::move(grid));
    const PlayerId id = *engine.addPlayer();
    Bot* bot = control(engine, id, BotDifficulty::Hard);

    run(engine, 1);

    EXPECT_EQ(bot->getIntent(), Bot::Intent::Idle);
    EXPECT_EQ(bot->getTarget(), std::nullopt);
    EXPECT_EQ(bot->getProfile().reactionTicks, 0);
}

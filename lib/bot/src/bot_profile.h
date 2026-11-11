#ifndef BOT_PROFILE_H
#define BOT_PROFILE_H

#pragma once

#include <array>

namespace Bomberman
{

// How strong the computer-controlled characters are, picked in the setup
// menu. Every level uses exactly the same brain (Bot); only the BotProfile
// below changes, see doc/bot.md.
enum class BotDifficulty
{
    Easy,
    Normal,
    Hard
};

inline constexpr int BotDifficultyCount = 3;
inline constexpr std::array<BotDifficulty, BotDifficultyCount> AllBotDifficulties = {
    BotDifficulty::Easy, BotDifficulty::Normal, BotDifficulty::Hard};

// The knobs of a bot. Durations are in simulation ticks (60 per second),
// chances in percent (0-100), values are the weights the bot gives to each
// kind of goal when it picks where to go (0 = never a goal).
struct BotProfile
{
    // Bombs dropped by others are only noticed once they have been ticking
    // for this long (the bot's own bombs are always known): the bot reacts
    // later to a new threat. 0 = sees everything at once.
    int reactionTicks = 0;

    // Outside danger, chance per decision to stay idle for hesitationTicks
    // (a human-like pause; never while in danger).
    int hesitationPercent = 0;
    int hesitationTicks = 0;

    // Chance per decision to head for a random goal instead of the best one.
    int mistakePercent = 0;

    // Bombs it is willing to have on the grid at once, its stats permitting.
    int maxBombs = 1;

    // Goal values.
    int blockValue = 10;   // per destructible block a bomb dropped there would destroy
    int upgradeValue = 0;  // an upgrade lying there (ignored once that stat is capped)
    int attackValue = 0;   // per opponent standing in the blast of a bomb dropped there (pressure)
    // A bomb dropped there would leave an opponent no way out (checked
    // with the opponent's own ReachMap, as if it played perfectly). 0 = the
    // bot never looks for traps.
    int trapValue = 0;

    // Before dropping a bomb, also makes sure it could still get away if
    // every opponent within this many cells dropped one on the cell it
    // faces right now (0 = does not anticipate).
    int anticipationRange = 0;

    // Punches bombs so that they slide onto opponents.
    bool pushesBombs = false;
    // With nothing better to do: walk towards the nearest opponent (true) or
    // wander around (false).
    bool huntsOpponents = false;

    // The preset of a difficulty level.
    [[nodiscard]] static constexpr BotProfile forDifficulty(BotDifficulty difficulty) noexcept
    {
        switch (difficulty)
        {
        case BotDifficulty::Easy:
            return BotProfile{30, 30, 24, 25, 1, 10, 5, 5, 0, 0, false, false};
        case BotDifficulty::Normal:
            return BotProfile{12, 8, 12, 8, 2, 10, 25, 30, 0, 0, false, true};
        case BotDifficulty::Hard:
            break;
        }
        return BotProfile{0, 0, 0, 0, 8, 10, 40, 30, 100, 4, true, true};
    }
};

} // namespace Bomberman

#endif // BOT_PROFILE_H

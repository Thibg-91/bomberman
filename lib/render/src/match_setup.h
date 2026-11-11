#ifndef MATCH_SETUP_H
#define MATCH_SETUP_H

#pragma once

#include <array>
#include <vector>

#include "character_model.h"

namespace Bomberman
{

// How the match is played.
enum class GameMode
{
    VersusComputer, // the keyboard player against computer-controlled pirates
    Host,           // host a network game on the chosen map (waiting room, see doc/network.md)
    Join            // join a network game hosted on the local network
};

inline constexpr int GameModeCount = 3;
inline constexpr std::array<GameMode, GameModeCount> AllGameModes = {GameMode::VersusComputer, GameMode::Host,
                                                                     GameMode::Join};

// What the launch menu sets up (validated mock-up "Préparer l'abordage",
// layout "Le tableau de bord"): one row per setting, the keyboard moves
// between rows (focus) and changes the value of the focused one. Pure
// logic, so it is tested without a window; MatchSetupRenderer draws it and
// exe/main.cpp turns it into a match.
struct MatchSetup
{
    // Rows from top to bottom (focus order).
    enum class Row
    {
        Mode,
        Pirate,
        Map,
        Difficulty,
        Opponents
    };

    static constexpr int RowCount = 5;
    static constexpr int DifficultyCount = 3; // easy, normal, hard (BotDifficulty order)
    static constexpr int MaxOpponents = 3;    // every shipped map has 4 spawn points

    GameMode mode = GameMode::VersusComputer;
    CharacterModel pirate = CharacterModel::Buccaneer;
    int map = 0;      // index in the menu's maps
    int mapCount = 0; // how many maps the menu offers
    int difficulty = 1;
    int opponents = MaxOpponents; // 1..MaxOpponents
    Row focus = Row::Pirate;

    // Focus on the next row (step 1, down) or the previous one (-1, up),
    // wrapping around and skipping the rows the mode does not use.
    void moveFocus(int step) noexcept;

    // Whether the mode uses row: the map is the host's when joining, the
    // difficulty and the opponents only matter against the computer
    // (network games are between humans only).
    [[nodiscard]] bool isRowActive(Row row) const noexcept;

    // Changes the value of the focused row by step (left -1, right 1),
    // wrapping around.
    void change(int step) noexcept;

    // Whether the launch button works: there is a map to play on (joining
    // needs none, the host chooses it).
    [[nodiscard]] bool canStart() const noexcept;

    // Index of value after moving by step in [0, count), wrapping around.
    [[nodiscard]] static int wrap(int value, int step, int count) noexcept;
};

// Models of the computer-controlled pirates: the rest of the crew, in crew
// order, skipping the player's own model.
[[nodiscard]] std::vector<CharacterModel> opponentModels(CharacterModel player, int count);

} // namespace Bomberman

#endif // MATCH_SETUP_H

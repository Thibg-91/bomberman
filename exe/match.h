#ifndef EXE_MATCH_H
#define EXE_MATCH_H

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

#include "bomb_renderer.h"
#include "character_model.h"
#include "character_renderer.h"
#include "danger_zone_renderer.h"
#include "explosion_renderer.h"
#include "game_engine.h"
#include "game_event.h"
#include "game_over_renderer.h"
#include "grid.h"
#include "grid_renderer.h"
#include "match_referee.h"
#include "match_setup.h"
#include "protocol.h"
#include "tick_playout.h"
#include "upgrade_renderer.h"

namespace Bomberman
{

// Who drives the engine of a match on this computer.
enum class MatchRole
{
    Solo,  // the keyboard player against bots; the engine runs on the frame clock
    Host,  // network: runs the engine, feeds the clients' commands, relays what ran
    Client // network: replays the host's ticks (TickPlayout), never advances on its own
};

// Everything that belongs to one round; rebuilt from scratch for each
// match. The engine runs the game logic at a fixed tick rate (the bots
// think inside it, see Bot), the renderers only display its state.
struct Match
{
    MatchRole role = MatchRole::Solo;
    GameEngine engine;
    PlayerId player = 0; // the character driven by this computer's keyboard
    GridRenderer gridRenderer;
    DangerZoneRenderer dangerZoneRenderer;
    UpgradeRenderer upgradeRenderer;
    std::vector<CharacterRenderer> characterRenderers; // indexed by PlayerId
    std::vector<CharacterModel> models;                // indexed by PlayerId
    std::vector<CharacterRenderer::Palette> palettes;  // indexed by PlayerId
    ExplosionRenderer explosionRenderer;
    MatchReferee referee;
    GameOverRenderer gameOver;
    std::vector<BombRenderer> bombRenderers;
    TickPlayout playout;         // Client only
    bool desynchronised = false; // Client only: a checksum differed (reported once)
    float overFor = 0.f;         // real seconds since the match was over
};

// The model at index in AllCharacterModels (the network sends indexes),
// clamped.
[[nodiscard]] CharacterModel modelAt(int index) noexcept;
[[nodiscard]] int indexOfModel(CharacterModel model) noexcept;

// Tile size filling as much of the screen as possible with square tiles.
[[nodiscard]] float tileSizeFor(const Grid& grid, sf::Vector2u screenSize) noexcept;

// The keyboard player on the first spawn point, then one bot per opponent
// on the next ones (as many as the map has room for).
[[nodiscard]] std::unique_ptr<Match> startSoloMatch(const Grid& grid, const MatchSetup& setup, std::uint64_t seed,
                                                    sf::Vector2u screenSize);

// A network match as the host described it: same grid, rules and players
// on every computer; each player wears the color of its seat. The referee
// never speeds up nor cuts the match short (the others are humans too).
[[nodiscard]] std::unique_ptr<Match> startNetworkMatch(const StartMatch& start, MatchRole role,
                                                       sf::Vector2u screenSize);

// The grid rarely fills the screen exactly (its aspect ratio differs from
// the screen's), so center it instead of leaving it stuck in a corner.
[[nodiscard]] sf::View centeredView(const Match& match, sf::Vector2u screenSize);

// Reacts to what happened during the frame's ticks (animations, grid
// rebuilt after explosions), updates the renderers and the referee.
void presentFrame(Match& match, const std::vector<GameEvent>& events, float deltaSeconds);

// Draws the board (grid, danger zones, upgrades, bombs, characters,
// explosions) and, once the match is over, the end screen. Returns true
// once the end screen has been shown long enough (time to leave).
bool drawMatch(sf::RenderWindow& window, Match& match, sf::Vector2u screenSize, float deltaSeconds);

} // namespace Bomberman

#endif // EXE_MATCH_H

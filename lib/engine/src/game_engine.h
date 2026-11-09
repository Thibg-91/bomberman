#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "bomb_manager.h"
#include "character.h"
#include "character_command.h"
#include "fixed_timestep.h"
#include "game_event.h"
#include "game_rules.h"
#include "grid.h"

namespace Bomberman
{

// Runs a match: owns the grid, the bombs and every character, and advances
// them by fixed ticks (GameRules::tickRate per second). Purely logical (no
// SFML), so it is fully testable headless.
//
// Characters are only driven by commands: whoever controls a player (the
// keyboard today, an AI or a remote peer tomorrow) calls submitCommand()
// with that player's id, the engine treats every player the same way. After
// an action a player is busy for a few ticks (GameRules) and the commands it
// receives meanwhile are dropped, which paces the game: an action never
// follows another one instantly.
//
// Order of one tick (deterministic, players by increasing id):
//   1. every ready, alive player with a pending command executes it;
//   2. characters walk and cooldowns count down;
//   3. bomb fuses burn; explosions destroy blocks and kill the players
//      standing in their blast.
class GameEngine
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    explicit GameEngine(Grid grid, GameRules rules = {});

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] const GameRules& getRules() const noexcept;
    [[nodiscard]] const Grid& getGrid() const noexcept;
    [[nodiscard]] const BombManager& getBombManager() const noexcept;

    // Number of ticks run since the match started.
    [[nodiscard]] std::uint64_t getTick() const noexcept;

    // Adds a player on the next unused spawn point of the grid (in the
    // grid's order) and returns its id (0, 1, 2...), or nothing when every
    // spawn point is taken.
    std::optional<PlayerId> addPlayer();

    [[nodiscard]] int getPlayerCount() const noexcept;
    [[nodiscard]] bool hasPlayer(PlayerId id) const noexcept;
    [[nodiscard]] int countAlivePlayers() const noexcept;

    // Throw std::out_of_range for an unknown id.
    [[nodiscard]] const Character& getCharacter(PlayerId id) const;
    // Ticks left before the player can act again (0 when it can).
    [[nodiscard]] int getCooldownTicks(PlayerId id) const;
    // Whether a command submitted now would be executed at the next tick:
    // alive, not walking and not cooling down.
    [[nodiscard]] bool isReady(PlayerId id) const;

    // Asks player id to execute command at the next tick. A later
    // submission before that tick replaces it; unknown ids are ignored. The
    // command is then dropped: it is an intent for "now", it is never
    // queued for when the player becomes ready (holding a key submits it
    // again every frame).
    void submitCommand(PlayerId id, const CharacterCommand& command);

    // Adds real elapsed time and runs as many ticks as it covers (see
    // FixedTimestep); returns how many ran. The commands submitted before
    // the call stay pending for all of these ticks (like a key held during
    // the whole frame), so a frame rate below the tick rate does not make a
    // player miss the tick where it becomes ready. They are dropped
    // afterwards.
    int advance(float realSeconds);

    // Runs exactly one tick; pending commands are dropped afterwards.
    void tick();

    // Part of a tick waiting to be simulated, in [0, 1).
    [[nodiscard]] float getInterpolationAlpha() const noexcept;

    // Events recorded since the last call (oldest first); clears them.
    [[nodiscard]] std::vector<GameEvent> takeEvents();

private:
    struct Player
    {
        PlayerId id;
        Character character;
        int cooldownTicks = 0;
        std::optional<CharacterCommand> pendingCommand;
    };

    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] const Player& player(PlayerId id) const;
    [[nodiscard]] static bool isReady(const Player& player) noexcept;
    void runTick();
    void clearCommands() noexcept;
    void execute(Player& player, const CharacterCommand& command);
    void emit(GameEvent::Type type, PlayerId id, Position position, std::vector<Position> cells = {});

    // =========================
    // Members
    // =========================
    GameRules m_rules;
    Grid m_grid;
    BombManager m_bombManager;
    FixedTimestep m_timestep;
    std::vector<Player> m_players;
    std::vector<GameEvent> m_events;
    std::uint64_t m_tick = 0;
};

} // namespace Bomberman

#endif // GAME_ENGINE_H

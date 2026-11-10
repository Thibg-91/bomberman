#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "bomb_manager.h"
#include "character.h"
#include "character_command.h"
#include "fixed_timestep.h"
#include "game_event.h"
#include "game_rules.h"
#include "grid.h"
#include "player_controller.h"
#include "player_stats.h"
#include "random.h"
#include "upgrade.h"
#include "upgrade_manager.h"

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
// Players can also be driven from inside the simulation by a
// PlayerController (bots): see setController().
//
// Order of one tick (deterministic, players by increasing id):
//   1. every ready, alive player executes its command: the one its
//      controller returns if it has one, otherwise its pending one;
//   2. alive players pick up the upgrade on their cell (as soon as a move
//      starts, since the logical position changes at once);
//   3. characters walk and cooldowns count down;
//   4. bomb fuses burn; each explosion is reported (one Explosion event per
//      bomb, with its Blast); explosions destroy blocks and the upgrades lying in
//      their blast, destroyed blocks may reveal new upgrades (spared by the
//      explosion that revealed them), and the players standing in a blast
//      are killed.
//
// Upgrades (GameRules::upgrades) raise the PlayerStats of whoever picks them
// up: bombs on the grid at once, push distance, blast radius of the bombs
// it drops afterwards.
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
    [[nodiscard]] const std::vector<Upgrade>& getUpgrades() const noexcept;

    // Puts an upgrade on the grid (tests, a future level editor...). Fails
    // if the cell is not walkable or already holds one.
    bool addUpgrade(Upgrade upgrade);

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
    // Current abilities of the player (starting stats + upgrades).
    [[nodiscard]] const PlayerStats& getStats(PlayerId id) const;
    // Ticks left before the player can act again (0 when it can).
    [[nodiscard]] int getCooldownTicks(PlayerId id) const;
    // Whether a command submitted now would be executed at the next tick:
    // alive, not walking and not cooling down.
    [[nodiscard]] bool isReady(PlayerId id) const;

    // Hands player id over to controller (nullptr gives it back to
    // submitCommand()): from then on, at every tick where the player is
    // ready, the engine asks the controller what to do and ignores the
    // submitted commands. Unknown ids are ignored.
    void setController(PlayerId id, std::unique_ptr<PlayerController> controller);
    [[nodiscard]] bool hasController(PlayerId id) const;

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
        PlayerStats stats;
        int cooldownTicks = 0;
        std::optional<CharacterCommand> pendingCommand;
        std::unique_ptr<PlayerController> controller;
    };

    // =========================
    // Private Methods
    // =========================
    [[nodiscard]] const Player& player(PlayerId id) const;
    [[nodiscard]] static bool isReady(const Player& player) noexcept;
    void runTick();
    void clearCommands() noexcept;
    void execute(Player& player, const CharacterCommand& command);
    // Cells of the other alive players, which a pushed bomb cannot slide into.
    [[nodiscard]] std::vector<Position> otherPlayerPositions(const Player& player) const;
    void collectUpgrades();
    void updateBombs(float deltaSeconds);
    void emit(GameEvent::Type type, PlayerId id, Position position);
    void emit(GameEvent::Type type, PlayerId id, const Upgrade& upgrade);
    void emitExplosion(const Blast& blast);

    // =========================
    // Members
    // =========================
    GameRules m_rules;
    Grid m_grid;
    BombManager m_bombManager;
    UpgradeManager m_upgradeManager;
    Random m_random;
    FixedTimestep m_timestep;
    std::vector<Player> m_players;
    std::vector<GameEvent> m_events;
    std::uint64_t m_tick = 0;
};

} // namespace Bomberman

#endif // GAME_ENGINE_H

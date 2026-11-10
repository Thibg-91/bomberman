#ifndef BOT_H
#define BOT_H

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "bomb_manager.h"
#include "bot_profile.h"
#include "character_command.h"
#include "danger_map.h"
#include "game_engine.h"
#include "player_controller.h"
#include "position.h"
#include "random.h"
#include "reach_map.h"

namespace Bomberman
{

// A computer-controlled character. The engine asks it what to do at every
// tick where its player is ready (see PlayerController); it answers with an
// ordinary CharacterCommand, so it plays by exactly the same rules as a
// human. The full design is explained in doc/bot.md. In short, each
// decision goes down this list and stops at the first rule that applies:
//
//   1. Escape: if an explosion it knows about is going to hit its cell,
//      punch the bomb in front of it onto an opponent (hard level), or take
//      the first step towards the nearest cell it can reach and stay on for
//      good (ReachMap over the DangerMap), or punch a bomb away when there
//      is no way out.
//   2. Hesitate (easy levels): sometimes stay idle for a moment.
//   3. Act where it stands: drop a bomb on its own cell if that is worth
//      it (blocks to destroy, an opponent in the blast or trapped) and it
//      could still escape afterwards (stepping off the bomb).
//   4. Go somewhere: score every cell it can safely reach (upgrades, spots
//      to bomb blocks or opponents from) by value / travel time and take
//      the first step towards the best one.
//   5. Otherwise hunt the nearest opponent or wander around.
//
// The BotProfile (from the difficulty) tunes how fast it notices bombs,
// how often it hesitates or errs, what it values and which tricks it uses.
// Its randomness comes from its own Random, seeded at construction: the
// same seed and the same match give the same decisions.
class Bot : public PlayerController
{
public:
    // What the last decision was about (tests, debugging).
    enum class Intent
    {
        Idle,        // nothing worth doing
        Escape,      // getting out of a blast
        Hesitate,    // staying idle on purpose (easy levels)
        PlaceBomb,   // dropping a bomb where it stands
        PushBomb,    // punching a bomb (at an opponent, or to clear its way out)
        Collect,     // heading for an upgrade
        BreakBlocks, // heading for a spot to bomb blocks from
        Attack,      // heading for a spot to bomb an opponent from
        Hunt,        // walking towards the nearest opponent
        Wander       // walking around
    };

    // How long (ticks) an idle bot waits before thinking again, unless a
    // bomb threatens it meanwhile.
    static constexpr int IdleRethinkTicks = 6;
    // How long (ticks) a spot where dropping a bomb turned out to be unsafe
    // is left aside.
    static constexpr int RejectTicks = 60;
    // Trap checks are costly (one ReachMap per opponent): only the best
    // attack goals of a decision get one.
    static constexpr int TrapChecksPerDecision = 3;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit Bot(BotProfile profile, std::uint64_t seed = Random::DefaultSeed) noexcept;
    explicit Bot(BotDifficulty difficulty, std::uint64_t seed = Random::DefaultSeed) noexcept;

    // =========================
    // Public Methods
    // =========================
    std::optional<CharacterCommand> decide(const GameEngine& engine, PlayerId id) override;

    [[nodiscard]] const BotProfile& getProfile() const noexcept;
    [[nodiscard]] Intent getIntent() const noexcept;
    // The cell the last decision was heading for, if any.
    [[nodiscard]] std::optional<Position> getTarget() const noexcept;

    // The explosions player id is aware of: its own bombs and the other
    // ones that have been ticking for at least reactionTicks. bombs is
    // usually engine.getBombManager(), or a copy with a hypothetical bomb.
    [[nodiscard]] static DangerMap perceive(const GameEngine& engine, PlayerId id, int reactionTicks,
                                            const BombManager& bombs);

private:
    // =========================
    // Private Methods
    // =========================
    // What a bomb dropped on cell now would be worth for player id (blocks,
    // opponents, upgrades destroyed), given the explosions already coming.
    [[nodiscard]] int bombValue(const GameEngine& engine, PlayerId id, const DangerMap& danger, Position cell,
                                bool* hitsOpponent = nullptr) const;
    // Whether an opponent that cannot escape a bomb on cell is near it.
    [[nodiscard]] bool trapsAnOpponent(const GameEngine& engine, PlayerId id, Position cell) const;
    // Whether player id could still get away after dropping a bomb on cell.
    [[nodiscard]] bool canEscapeAfterPlacing(const GameEngine& engine, PlayerId id, Position cell) const;
    // Whether player id, once the bombs are as in bombs and it has spent
    // delay ticks on its cell, can still reach a safe cell.
    [[nodiscard]] bool canEscapeWith(const GameEngine& engine, PlayerId id, const BombManager& bombs,
                                     int delay) const;

    // A decision: the command to execute now, or nothing to wait this tick.
    using Decision = std::optional<CharacterCommand>;

    // The rules of decide(), in order. The ones returning an optional
    // Decision return nothing when they do not apply.
    Decision escape(const GameEngine& engine, PlayerId id, const DangerMap& danger);
    std::optional<Decision> pushOntoOpponent(const GameEngine& engine, PlayerId id);
    std::optional<Decision> actWhereItStands(const GameEngine& engine, PlayerId id, const DangerMap& danger,
                                             bool canBomb);
    std::optional<Decision> goSomewhere(const GameEngine& engine, PlayerId id, const DangerMap& danger,
                                        const ReachMap& reach, bool canBomb);
    Decision roam(const GameEngine& engine, PlayerId id, const ReachMap& reach);

    [[nodiscard]] bool isRejected(Position cell, std::uint64_t tick) const;
    void reject(Position cell, std::uint64_t tick);
    Decision headFor(const ReachMap& reach, Position cell, Intent intent);
    Decision idle(std::uint64_t tick);

    // =========================
    // Members
    // =========================
    struct Rejected
    {
        Position cell;
        std::uint64_t until;
    };

    BotProfile m_profile;
    Random m_random;
    Intent m_intent = Intent::Idle;
    std::optional<Position> m_target;
    std::optional<Position> m_wanderTarget;
    std::uint64_t m_idleUntil = 0;
    std::vector<Rejected> m_rejected;
};

} // namespace Bomberman

#endif // BOT_H

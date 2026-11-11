#ifndef MATCH_REFEREE_H
#define MATCH_REFEREE_H

#pragma once

#include <optional>

#include "game_engine.h"

namespace Bomberman
{

// Decides when a match is over and who won, from the point of view of the
// keyboard player (the human):
//   - Playing: the human is alive and has opponents left;
//   - Spectating: the human is dead but several opponents are still alive:
//     the match goes on SpectatorSpeed times faster until one of them is
//     left, or for at most SpectatorTimeLimit real seconds (then a draw);
//   - Over: one player (or nobody) is left: the engine stops (gameSeconds()
//     returns 0) and the winner is known.
// Purely logical; exe/main.cpp asks it how much game time to run each frame
// and shows the end screen once it is over.
class MatchReferee
{
public:
    static constexpr float SpectatorSpeed = 4.f;
    static constexpr float SpectatorTimeLimit = 30.f; // real seconds

    enum class Phase
    {
        Playing,
        Spectating,
        Over
    };

    // =========================
    // Constructors / Destructor
    // =========================
    explicit MatchReferee(PlayerId human) noexcept;

    // =========================
    // Public Methods
    // =========================
    // Game time to give GameEngine::advance() for a frame of realSeconds.
    [[nodiscard]] float gameSeconds(float realSeconds) const noexcept;

    // Looks at the match after a frame that lasted realSeconds.
    void update(const GameEngine& engine, float realSeconds);

    [[nodiscard]] Phase getPhase() const noexcept;
    [[nodiscard]] bool isOver() const noexcept;
    // The last player standing once the match is over; nothing for a draw
    // (everybody died, or the spectating time ran out) or while it goes on.
    [[nodiscard]] std::optional<PlayerId> getWinner() const noexcept;
    [[nodiscard]] PlayerId getHuman() const noexcept;

private:
    // =========================
    // Members
    // =========================
    PlayerId m_human;
    Phase m_phase = Phase::Playing;
    std::optional<PlayerId> m_winner;
    float m_spectated = 0.f; // real seconds spent spectating
};

} // namespace Bomberman

#endif // MATCH_REFEREE_H

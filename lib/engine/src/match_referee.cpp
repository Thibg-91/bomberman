#include "match_referee.h"

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
MatchReferee::MatchReferee(PlayerId human) noexcept
    : m_human(human)
{
}

// =========================
// Public Methods
// =========================
float MatchReferee::gameSeconds(float realSeconds) const noexcept
{
    switch (m_phase)
    {
    case Phase::Playing:
        return realSeconds;
    case Phase::Spectating:
        return realSeconds * SpectatorSpeed;
    case Phase::Over:
        break;
    }
    return 0.f;
}

void MatchReferee::update(const GameEngine& engine, float realSeconds)
{
    if (m_phase == Phase::Over)
        return;

    const int alive = engine.countAlivePlayers();
    const bool alone = engine.getPlayerCount() <= 1;
    // Decided: one player (or nobody) left, or the human alone on a map
    // without opponents died.
    if ((!alone && alive <= 1) || alive == 0)
    {
        m_phase = Phase::Over;
        for (PlayerId id = 0; id < engine.getPlayerCount() && alive == 1 && !alone; ++id)
            if (engine.getCharacter(id).isAlive())
                m_winner = id;
        return;
    }

    if (engine.hasPlayer(m_human) && !engine.getCharacter(m_human).isAlive())
    {
        if (m_phase == Phase::Spectating)
            m_spectated += realSeconds;
        m_phase = Phase::Spectating;
        if (m_spectated >= SpectatorTimeLimit)
            m_phase = Phase::Over; // nobody won in time: a draw
    }
}

MatchReferee::Phase MatchReferee::getPhase() const noexcept
{
    return m_phase;
}

bool MatchReferee::isOver() const noexcept
{
    return m_phase == Phase::Over;
}

std::optional<PlayerId> MatchReferee::getWinner() const noexcept
{
    return m_winner;
}

PlayerId MatchReferee::getHuman() const noexcept
{
    return m_human;
}

} // namespace Bomberman

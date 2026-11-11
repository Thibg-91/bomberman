#include "match_referee.h"

#include <algorithm>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
MatchReferee::MatchReferee(PlayerId human, SpectatorRule rule) noexcept
    : m_human(human),
      m_rule(rule)
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
        return m_rule == SpectatorRule::FastForward ? realSeconds * SpectatorSpeed : realSeconds;
    case Phase::Over:
        break;
    }
    return 0.f;
}

void MatchReferee::update(const GameEngine& engine, float realSeconds, const std::vector<GameEvent>& events)
{
    if (m_phase == Phase::Over)
        return;

    const int playerCount = engine.getPlayerCount();
    const bool alone = playerCount <= 1;
    // Decided: one player (or nobody) left, or the human alone on a map
    // without opponents died.
    auto decided = [alone](int alive) { return (!alone && alive <= 1) || alive == 0; };

    // Who was alive at the start of the frame: the living plus the players
    // killed during it. Then replay the deaths tick by tick, so the verdict
    // is the one of the first decisive tick.
    std::vector<bool> alive(static_cast<std::size_t>(std::max(playerCount, 0)), false);
    for (PlayerId id = 0; id < playerCount; ++id)
        alive[static_cast<std::size_t>(id)] = engine.getCharacter(id).isAlive();
    std::vector<const GameEvent*> kills;
    for (const GameEvent& event : events)
        if (event.type == GameEvent::Type::PlayerKilled && engine.hasPlayer(event.playerId))
        {
            kills.push_back(&event);
            alive[static_cast<std::size_t>(event.playerId)] = true;
        }
    std::stable_sort(kills.begin(), kills.end(),
                     [](const GameEvent* lhs, const GameEvent* rhs) { return lhs->tick < rhs->tick; });

    auto countAlive = [&alive]() { return static_cast<int>(std::count(alive.begin(), alive.end(), true)); };
    auto finish = [&]() {
        m_phase = Phase::Over;
        if (!alone && countAlive() == 1)
            m_winner = static_cast<PlayerId>(std::find(alive.begin(), alive.end(), true) - alive.begin());
    };

    if (decided(countAlive()))
    {
        finish(); // already decided before this frame's deaths
        return;
    }
    for (std::size_t i = 0; i < kills.size();)
    {
        const std::uint64_t tick = kills[i]->tick;
        for (; i < kills.size() && kills[i]->tick == tick; ++i)
            alive[static_cast<std::size_t>(kills[i]->playerId)] = false;
        if (decided(countAlive()))
        {
            finish();
            return;
        }
    }
    // Deaths the events did not report (callers that pass none).
    for (PlayerId id = 0; id < playerCount; ++id)
        alive[static_cast<std::size_t>(id)] = engine.getCharacter(id).isAlive();
    if (decided(countAlive()))
    {
        finish();
        return;
    }

    if (engine.hasPlayer(m_human) && !engine.getCharacter(m_human).isAlive())
    {
        if (m_phase == Phase::Spectating)
            m_spectated += realSeconds;
        m_phase = Phase::Spectating;
        if (m_rule == SpectatorRule::FastForward && m_spectated >= SpectatorTimeLimit)
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

MatchReferee::SpectatorRule MatchReferee::getSpectatorRule() const noexcept
{
    return m_rule;
}

} // namespace Bomberman

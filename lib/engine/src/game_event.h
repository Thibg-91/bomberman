#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#pragma once

#include <cstdint>
#include <vector>

#include "position.h"

namespace Bomberman
{

using PlayerId = int; // also the owner id of the player's bombs

// Something that happened during a tick. The engine only records them;
// presentation code (renderers, sounds, a future network layer...) reads
// them with GameEngine::takeEvents() to react without polling the state.
struct GameEvent
{
    enum class Type
    {
        PlayerTurned,  // playerId now faces another direction
        PlayerMoved,   // playerId started walking to position
        BombPlaced,    // playerId dropped a bomb on position
        BombPushed,    // playerId punched a bomb, which now lies on position
        Explosion,     // bombs exploded; cells = every cell their blasts hit (bomb centers included)
        PlayerKilled   // playerId was caught in an explosion on position
    };

    Type type = Type::PlayerMoved;
    std::uint64_t tick = 0; // tick during which it happened
    PlayerId playerId = -1; // -1 when no player is involved (Explosion)
    Position position;
    std::vector<Position> cells; // only for Explosion
};

} // namespace Bomberman

#endif // GAME_EVENT_H

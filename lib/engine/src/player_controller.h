#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#pragma once

#include <optional>

#include "character_command.h"

namespace Bomberman
{

class GameEngine;
using PlayerId = int; // also the owner id of the player's bombs

// Something that drives a player from inside the simulation (a bot today).
// Unlike the keyboard, which submits commands once per frame, a controller
// is asked by the engine itself at every tick where its player is ready
// (alive, not walking, not cooling down), so it plays exactly the same
// whatever the frame rate is: same seed + same commands = same game.
//
// It reads the match through the const engine and returns the command to
// execute right now, or nothing to stay idle this tick (it is asked again
// at the next one). The command goes through the very same path as a
// submitted one: a controller can do nothing a keyboard player could not.
class PlayerController
{
public:
    virtual ~PlayerController() = default;

    // engine.getTick() is the tick being run. Players with a lower id have
    // already acted during this tick (their moves and bombs are visible).
    virtual std::optional<CharacterCommand> decide(const GameEngine& engine, PlayerId id) = 0;
};

} // namespace Bomberman

#endif // PLAYER_CONTROLLER_H

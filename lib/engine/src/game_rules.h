#ifndef GAME_RULES_H
#define GAME_RULES_H

#pragma once

#include "bomb_manager.h"

namespace Bomberman
{

// Pacing and gameplay settings of a match. Every delay is a whole number of
// simulation ticks so the game plays exactly the same whatever the frame
// rate is (a prerequisite for replays and lock-step network play).
//
// Tick rate: 60 Hz. It matches the display rate the window is capped to (one
// tick per frame most of the time, so movement stays smooth), keeps input
// latency under 17 ms, and gives a fine enough grain to tune delays (one tick
// = 16.7 ms). The logic is cheap enough for this rate to be affordable even
// with 4 characters, and a future network layer can still exchange inputs
// every few ticks without changing the simulation rate.
struct GameRules
{
    static constexpr int DefaultTickRate = 60; // simulation ticks per second

    int tickRate = DefaultTickRate;

    // Delays before the character can act again, by action. A move also
    // sets how long the character takes to walk one cell (renderers
    // interpolate during that time).
    int moveTicks = 10;      // ~0.17 s per cell
    int turnTicks = 5;       // ~0.08 s to face another direction
    int placeBombTicks = 15; // 0.25 s after dropping a bomb
    int pushBombTicks = 15;  // 0.25 s after punching a bomb

    int maxActiveBombs = BombManager::DefaultMaxActiveBombs; // per character
    int pushDistance = BombManager::DefaultPushDistance;     // cells per punch

    [[nodiscard]] constexpr float tickDuration() const noexcept
    {
        return 1.f / static_cast<float>(tickRate);
    }

    [[nodiscard]] constexpr float secondsFor(int ticks) const noexcept
    {
        return static_cast<float>(ticks) * tickDuration();
    }
};

} // namespace Bomberman

#endif // GAME_RULES_H

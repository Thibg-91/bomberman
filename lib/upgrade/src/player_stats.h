#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#pragma once

#include "upgrade.h"
#include "upgrade_rules.h"

namespace Bomberman
{

// Abilities of one character, raised by the upgrades it picks up.
struct PlayerStats
{
    int maxBombs = 1;     // bombs on the grid at once
    int pushDistance = 1; // cells a pushed bomb slides
    int blastRadius = 1;  // cells per arm of its bombs' explosions

    // Raises the stat matching type by one, up to the cap in rules.
    // Returns false (stats unchanged) when it is already at the cap.
    bool apply(UpgradeType type, const UpgradeRules& rules) noexcept;

    friend bool operator==(const PlayerStats& lhs, const PlayerStats& rhs) noexcept
    {
        return lhs.maxBombs == rhs.maxBombs && lhs.pushDistance == rhs.pushDistance
            && lhs.blastRadius == rhs.blastRadius;
    }

    friend bool operator!=(const PlayerStats& lhs, const PlayerStats& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace Bomberman

#endif // PLAYER_STATS_H

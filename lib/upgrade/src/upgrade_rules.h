#ifndef UPGRADE_RULES_H
#define UPGRADE_RULES_H

#pragma once

#include <array>

#include "upgrade.h"

namespace Bomberman
{

// How often upgrades show up and how far they can take a character.
struct UpgradeRules
{
    // Chance (in percent, 0-100) that a destroyed block hides an upgrade.
    int dropChancePercent = 25;
    // Relative chance of each type once an upgrade drops, indexed by
    // indexOf(UpgradeType). Equal weights: one chance in three each.
    std::array<int, UpgradeTypeCount> weights = {1, 1, 1};

    // Caps: an upgrade picked up at the cap is consumed without effect.
    int maxBombs = 8;
    int maxPushDistance = 5;
    int maxBlastRadius = 8;
};

} // namespace Bomberman

#endif // UPGRADE_RULES_H

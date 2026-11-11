#ifndef UPGRADE_H
#define UPGRADE_H

#pragma once

#include <array>

#include "position.h"

namespace Bomberman
{

// What a character gains by walking on an upgrade.
enum class UpgradeType
{
    ExtraBomb,    // +1 bomb on the grid at once
    PushStrength, // +1 cell when pushing a bomb
    BlastRadius   // +1 cell per arm of the character's explosions
};

inline constexpr int UpgradeTypeCount = 3;
inline constexpr std::array<UpgradeType, UpgradeTypeCount> AllUpgradeTypes = {
    UpgradeType::ExtraBomb, UpgradeType::PushStrength, UpgradeType::BlastRadius};

[[nodiscard]] constexpr int indexOf(UpgradeType type) noexcept
{
    return static_cast<int>(type);
}

// An upgrade lying on the grid, revealed by a destroyed block. Like bombs,
// it is an entity on top of the grid, not a tile: it never blocks movement,
// bombs or explosions.
struct Upgrade
{
    Position position;
    UpgradeType type = UpgradeType::ExtraBomb;

    friend bool operator==(const Upgrade& lhs, const Upgrade& rhs) noexcept
    {
        return lhs.position == rhs.position && lhs.type == rhs.type;
    }

    friend bool operator!=(const Upgrade& lhs, const Upgrade& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace Bomberman

#endif // UPGRADE_H

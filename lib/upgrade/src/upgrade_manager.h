#ifndef UPGRADE_MANAGER_H
#define UPGRADE_MANAGER_H

#pragma once

#include <optional>
#include <vector>

#include "grid.h"
#include "position.h"
#include "random.h"
#include "upgrade.h"
#include "upgrade_rules.h"

namespace Bomberman
{

// Owns the upgrades lying on the grid: they appear in destroyed blocks, are
// picked up by walking on them and are destroyed by later explosions. Purely
// logical; GameEngine decides when each of these happens.
class UpgradeManager
{
public:
    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] const std::vector<Upgrade>& getUpgrades() const noexcept;
    [[nodiscard]] std::optional<UpgradeType> getUpgradeAt(Position position) const noexcept;

    // Puts an upgrade on the grid (tests, a future level editor...). Fails
    // if the cell is not walkable or already holds an upgrade.
    bool add(Upgrade upgrade, const Grid& grid);

    // Removes the upgrade on position and returns its type, if any.
    std::optional<UpgradeType> takeAt(Position position);

    // Removes every upgrade standing on one of cells (e.g. an explosion's
    // hit cells) and returns them.
    std::vector<Upgrade> destroyAt(const std::vector<Position>& cells);

    // Draws, for each destroyed block, whether it hides an upgrade and of
    // which type (see roll()); the ones that do are added. Returns them, in
    // the order of destroyedBlocks.
    std::vector<Upgrade> spawnInDestroyedBlocks(const std::vector<Position>& destroyedBlocks, const Grid& grid,
                                                const UpgradeRules& rules, Random& random);

    // One draw: nothing with probability 1 - dropChancePercent, otherwise a
    // type picked according to rules.weights (nothing if they are all 0).
    [[nodiscard]] static std::optional<UpgradeType> roll(const UpgradeRules& rules, Random& random) noexcept;

private:
    // =========================
    // Members
    // =========================
    std::vector<Upgrade> m_upgrades;
};

} // namespace Bomberman

#endif // UPGRADE_MANAGER_H

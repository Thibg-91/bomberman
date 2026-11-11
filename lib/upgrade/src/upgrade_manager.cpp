#include "upgrade_manager.h"

#include <algorithm>
#include <iterator>

namespace Bomberman
{

// =========================
// Public Methods
// =========================
const std::vector<Upgrade>& UpgradeManager::getUpgrades() const noexcept
{
    return m_upgrades;
}

std::optional<UpgradeType> UpgradeManager::getUpgradeAt(Position position) const noexcept
{
    const auto it = std::find_if(m_upgrades.begin(), m_upgrades.end(),
                                 [position](const Upgrade& upgrade) { return upgrade.position == position; });
    if (it == m_upgrades.end())
        return std::nullopt;
    return it->type;
}

bool UpgradeManager::add(Upgrade upgrade, const Grid& grid)
{
    if (!grid.isWalkable(upgrade.position) || getUpgradeAt(upgrade.position))
        return false;
    m_upgrades.push_back(upgrade);
    return true;
}

std::optional<UpgradeType> UpgradeManager::takeAt(Position position)
{
    const auto it = std::find_if(m_upgrades.begin(), m_upgrades.end(),
                                 [position](const Upgrade& upgrade) { return upgrade.position == position; });
    if (it == m_upgrades.end())
        return std::nullopt;

    const UpgradeType type = it->type;
    m_upgrades.erase(it);
    return type;
}

std::vector<Upgrade> UpgradeManager::destroyAt(const std::vector<Position>& cells)
{
    std::vector<Upgrade> destroyed;
    const auto hit = [&cells](const Upgrade& upgrade) {
        return std::find(cells.begin(), cells.end(), upgrade.position) != cells.end();
    };
    std::copy_if(m_upgrades.begin(), m_upgrades.end(), std::back_inserter(destroyed), hit);
    m_upgrades.erase(std::remove_if(m_upgrades.begin(), m_upgrades.end(), hit), m_upgrades.end());
    return destroyed;
}

std::vector<Upgrade> UpgradeManager::spawnInDestroyedBlocks(const std::vector<Position>& destroyedBlocks,
                                                            const Grid& grid, const UpgradeRules& rules,
                                                            Random& random)
{
    std::vector<Upgrade> spawned;
    for (const Position block : destroyedBlocks)
    {
        // Always draw, even if the cell cannot take one, so the sequence of
        // draws only depends on the blocks destroyed.
        const std::optional<UpgradeType> type = roll(rules, random);
        if (type && add(Upgrade{block, *type}, grid))
            spawned.push_back(Upgrade{block, *type});
    }
    return spawned;
}

std::optional<UpgradeType> UpgradeManager::roll(const UpgradeRules& rules, Random& random) noexcept
{
    if (!random.percent(rules.dropChancePercent))
        return std::nullopt;

    int total = 0;
    for (const int weight : rules.weights)
        total += std::max(weight, 0);
    if (total == 0)
        return std::nullopt;

    int pick = random.below(total);
    for (const UpgradeType type : AllUpgradeTypes)
    {
        const int weight = std::max(rules.weights[static_cast<std::size_t>(indexOf(type))], 0);
        if (pick < weight)
            return type;
        pick -= weight;
    }
    return std::nullopt; // unreachable
}

} // namespace Bomberman

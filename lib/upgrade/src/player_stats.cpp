#include "player_stats.h"

namespace Bomberman
{

namespace
{

bool raise(int& stat, int cap) noexcept
{
    if (stat >= cap)
        return false;
    ++stat;
    return true;
}

} // namespace

// =========================
// Public Methods
// =========================
bool PlayerStats::apply(UpgradeType type, const UpgradeRules& rules) noexcept
{
    switch (type)
    {
    case UpgradeType::ExtraBomb:
        return raise(maxBombs, rules.maxBombs);
    case UpgradeType::PushStrength:
        return raise(pushDistance, rules.maxPushDistance);
    case UpgradeType::BlastRadius:
        return raise(blastRadius, rules.maxBlastRadius);
    }
    return false;
}

} // namespace Bomberman

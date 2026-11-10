#include <gtest/gtest.h>

#include "player_stats.h"

using Bomberman::PlayerStats;
using Bomberman::UpgradeRules;
using Bomberman::UpgradeType;

TEST(PlayerStatsTest, StartsWithOneOfEverything)
{
    const PlayerStats stats;

    EXPECT_EQ(stats.maxBombs, 1);
    EXPECT_EQ(stats.pushDistance, 1);
    EXPECT_EQ(stats.blastRadius, 1);
}

TEST(PlayerStatsTest, EachUpgradeRaisesItsOwnStatByOne)
{
    const UpgradeRules rules;
    PlayerStats stats;

    EXPECT_TRUE(stats.apply(UpgradeType::ExtraBomb, rules));
    EXPECT_EQ(stats, (PlayerStats{2, 1, 1}));

    EXPECT_TRUE(stats.apply(UpgradeType::PushStrength, rules));
    EXPECT_EQ(stats, (PlayerStats{2, 2, 1}));

    EXPECT_TRUE(stats.apply(UpgradeType::BlastRadius, rules));
    EXPECT_EQ(stats, (PlayerStats{2, 2, 2}));
}

TEST(PlayerStatsTest, StatsStopAtTheirCap)
{
    UpgradeRules rules;
    rules.maxBombs = 2;
    rules.maxPushDistance = 3;
    rules.maxBlastRadius = 1;
    PlayerStats stats;

    EXPECT_TRUE(stats.apply(UpgradeType::ExtraBomb, rules));
    EXPECT_FALSE(stats.apply(UpgradeType::ExtraBomb, rules));
    EXPECT_EQ(stats.maxBombs, 2);

    EXPECT_TRUE(stats.apply(UpgradeType::PushStrength, rules));
    EXPECT_TRUE(stats.apply(UpgradeType::PushStrength, rules));
    EXPECT_FALSE(stats.apply(UpgradeType::PushStrength, rules));
    EXPECT_EQ(stats.pushDistance, 3);

    EXPECT_FALSE(stats.apply(UpgradeType::BlastRadius, rules));
    EXPECT_EQ(stats.blastRadius, 1);
}

TEST(UpgradeRulesTest, DefaultsMatchTheGameDesign)
{
    const UpgradeRules rules;

    EXPECT_EQ(rules.dropChancePercent, 25);
    EXPECT_EQ(rules.weights[0], rules.weights[1]);
    EXPECT_EQ(rules.weights[1], rules.weights[2]);
    EXPECT_EQ(rules.maxBombs, 8);
    EXPECT_EQ(rules.maxPushDistance, 5);
    EXPECT_EQ(rules.maxBlastRadius, 8);
}

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "upgrade_manager.h"

using Bomberman::Grid;
using Bomberman::indexOf;
using Bomberman::Position;
using Bomberman::Random;
using Bomberman::Tile;
using Bomberman::TileType;
using Bomberman::Upgrade;
using Bomberman::UpgradeManager;
using Bomberman::UpgradeRules;
using Bomberman::UpgradeType;

namespace
{

UpgradeRules alwaysDrop(UpgradeType only)
{
    UpgradeRules rules;
    rules.dropChancePercent = 100;
    rules.weights = {0, 0, 0};
    rules.weights[static_cast<std::size_t>(indexOf(only))] = 1;
    return rules;
}

} // namespace

TEST(UpgradeManagerTest, StartsEmpty)
{
    const UpgradeManager manager;

    EXPECT_TRUE(manager.getUpgrades().empty());
    EXPECT_FALSE(manager.getUpgradeAt({1, 1}).has_value());
}

TEST(UpgradeManagerTest, AddPutsAnUpgradeOnAWalkableCell)
{
    const Grid grid(5, 5);
    UpgradeManager manager;

    EXPECT_TRUE(manager.add({{2, 2}, UpgradeType::BlastRadius}, grid));

    EXPECT_EQ(manager.getUpgradeAt({2, 2}), UpgradeType::BlastRadius);
    ASSERT_EQ(manager.getUpgrades().size(), 1u);
}

TEST(UpgradeManagerTest, AddFailsOnBlockedOrOccupiedCells)
{
    Grid grid(5, 5);
    grid.setTile({1, 1}, Tile(TileType::Wall));
    grid.setTile({3, 3}, Tile(TileType::DestructibleBlock));
    UpgradeManager manager;
    ASSERT_TRUE(manager.add({{2, 2}, UpgradeType::ExtraBomb}, grid));

    EXPECT_FALSE(manager.add({{1, 1}, UpgradeType::ExtraBomb}, grid));
    EXPECT_FALSE(manager.add({{3, 3}, UpgradeType::ExtraBomb}, grid));
    EXPECT_FALSE(manager.add({{9, 9}, UpgradeType::ExtraBomb}, grid));
    EXPECT_FALSE(manager.add({{2, 2}, UpgradeType::PushStrength}, grid));
    EXPECT_EQ(manager.getUpgrades().size(), 1u);
}

TEST(UpgradeManagerTest, TakeAtRemovesAndReturnsTheUpgrade)
{
    const Grid grid(5, 5);
    UpgradeManager manager;
    ASSERT_TRUE(manager.add({{2, 2}, UpgradeType::PushStrength}, grid));

    EXPECT_EQ(manager.takeAt({2, 2}), UpgradeType::PushStrength);
    EXPECT_FALSE(manager.takeAt({2, 2}).has_value());
    EXPECT_TRUE(manager.getUpgrades().empty());
}

TEST(UpgradeManagerTest, DestroyAtRemovesOnlyTheUpgradesOnTheCells)
{
    const Grid grid(5, 5);
    UpgradeManager manager;
    ASSERT_TRUE(manager.add({{1, 1}, UpgradeType::ExtraBomb}, grid));
    ASSERT_TRUE(manager.add({{2, 2}, UpgradeType::BlastRadius}, grid));
    ASSERT_TRUE(manager.add({{3, 3}, UpgradeType::PushStrength}, grid));

    const std::vector<Upgrade> destroyed = manager.destroyAt({{2, 2}, {3, 3}, {4, 4}});

    ASSERT_EQ(destroyed.size(), 2u);
    EXPECT_EQ(destroyed[0], (Upgrade{{2, 2}, UpgradeType::BlastRadius}));
    EXPECT_EQ(destroyed[1], (Upgrade{{3, 3}, UpgradeType::PushStrength}));
    ASSERT_EQ(manager.getUpgrades().size(), 1u);
    EXPECT_EQ(manager.getUpgrades().front().position, (Position{1, 1}));
}

TEST(UpgradeManagerTest, RollNeverDropsAtZeroPercent)
{
    UpgradeRules rules;
    rules.dropChancePercent = 0;
    Random random;

    for (int i = 0; i < 500; ++i)
        EXPECT_FALSE(UpgradeManager::roll(rules, random).has_value());
}

TEST(UpgradeManagerTest, RollNeverDropsWithoutWeights)
{
    UpgradeRules rules;
    rules.dropChancePercent = 100;
    rules.weights = {0, 0, 0};
    Random random;

    EXPECT_FALSE(UpgradeManager::roll(rules, random).has_value());
}

TEST(UpgradeManagerTest, RollFollowsTheWeights)
{
    const UpgradeRules rules = alwaysDrop(UpgradeType::PushStrength);
    Random random;

    for (int i = 0; i < 200; ++i)
        EXPECT_EQ(UpgradeManager::roll(rules, random), UpgradeType::PushStrength);
}

// A quarter of the destroyed blocks hide an upgrade, a third of them of each
// type.
TEST(UpgradeManagerTest, DefaultOddsAreOneInFourThenOneInThreeEach)
{
    const UpgradeRules rules;
    Random random(2024);
    constexpr int Draws = 60000;
    int drops = 0;
    std::array<int, Bomberman::UpgradeTypeCount> perType{};

    for (int i = 0; i < Draws; ++i)
    {
        if (const auto type = UpgradeManager::roll(rules, random))
        {
            ++drops;
            ++perType[static_cast<std::size_t>(indexOf(*type))];
        }
    }

    EXPECT_NEAR(drops, Draws / 4, Draws / 100); // 25 % +- 1 %
    for (const int count : perType)
        EXPECT_NEAR(count, drops / 3, drops / 50); // 33 % +- 2 % of the drops
}

TEST(UpgradeManagerTest, SpawnAddsTheDrawnUpgradesInTheDestroyedBlocks)
{
    const Grid grid(5, 5); // the blocks are already destroyed: empty cells
    UpgradeManager manager;
    Random random;

    const std::vector<Upgrade> spawned =
        manager.spawnInDestroyedBlocks({{1, 1}, {3, 1}}, grid, alwaysDrop(UpgradeType::ExtraBomb), random);

    ASSERT_EQ(spawned.size(), 2u);
    EXPECT_EQ(spawned[0], (Upgrade{{1, 1}, UpgradeType::ExtraBomb}));
    EXPECT_EQ(spawned[1], (Upgrade{{3, 1}, UpgradeType::ExtraBomb}));
    EXPECT_EQ(manager.getUpgrades(), spawned);
}

TEST(UpgradeManagerTest, SpawnIsDeterministicForAGivenSeed)
{
    const Grid grid(9, 9);
    std::vector<Position> blocks;
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 9; ++x)
            blocks.push_back({x, y});

    UpgradeManager first;
    UpgradeManager second;
    Random randomA(99);
    Random randomB(99);
    const UpgradeRules rules;

    EXPECT_EQ(first.spawnInDestroyedBlocks(blocks, grid, rules, randomA),
              second.spawnInDestroyedBlocks(blocks, grid, rules, randomB));
    EXPECT_FALSE(first.getUpgrades().empty());
    EXPECT_LT(first.getUpgrades().size(), blocks.size());
}

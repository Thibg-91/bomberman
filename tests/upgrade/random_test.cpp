#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "random.h"

using Bomberman::Random;

TEST(RandomTest, TheSameSeedGivesTheSameSequence)
{
    Random a(42);
    Random b(42);

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(a.next(), b.next());
}

TEST(RandomTest, DifferentSeedsGiveDifferentSequences)
{
    Random a(1);
    Random b(2);

    EXPECT_NE(a.next(), b.next());
}

// The generator's output is part of the game rules (same seed, same game on
// every platform): pin it so an accidental change is caught.
TEST(RandomTest, OutputIsStableAcrossPlatforms)
{
    Random random(0);

    EXPECT_EQ(random.next(), 0xE220A8397B1DCDAFull);
    EXPECT_EQ(random.next(), 0x6E789E6AA1B965F4ull);
}

TEST(RandomTest, BelowStaysInRange)
{
    Random random;
    for (int i = 0; i < 1000; ++i)
    {
        const int value = random.below(7);
        EXPECT_GE(value, 0);
        EXPECT_LT(value, 7);
    }
    EXPECT_EQ(random.below(0), 0);
    EXPECT_EQ(random.below(-3), 0);
}

TEST(RandomTest, BelowIsRoughlyUniform)
{
    Random random(7);
    std::array<int, 4> counts{};
    constexpr int Draws = 40000;
    for (int i = 0; i < Draws; ++i)
        ++counts[static_cast<std::size_t>(random.below(4))];

    for (const int count : counts)
        EXPECT_NEAR(count, Draws / 4, Draws / 100); // within 1 %
}

TEST(RandomTest, PercentHonoursItsBounds)
{
    Random random;
    for (int i = 0; i < 200; ++i)
    {
        EXPECT_FALSE(random.percent(0));
        EXPECT_FALSE(random.percent(-5));
        EXPECT_TRUE(random.percent(100));
        EXPECT_TRUE(random.percent(150));
    }
}

TEST(RandomTest, PercentMatchesItsProbability)
{
    Random random(3);
    constexpr int Draws = 40000;
    int hits = 0;
    for (int i = 0; i < Draws; ++i)
        hits += random.percent(25) ? 1 : 0;

    EXPECT_NEAR(hits, Draws / 4, Draws / 100);
}

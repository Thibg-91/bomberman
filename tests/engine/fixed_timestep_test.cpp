#include <gtest/gtest.h>

#include "fixed_timestep.h"

using Bomberman::FixedTimestep;

TEST(FixedTimestepTest, TickDurationFollowsTheTickRate)
{
    const FixedTimestep timestep(60);

    EXPECT_EQ(timestep.getTickRate(), 60);
    EXPECT_FLOAT_EQ(timestep.getTickDuration(), 1.f / 60.f);
    EXPECT_EQ(timestep.getMaxTicksPerFrame(), FixedTimestep::DefaultMaxTicksPerFrame);
}

TEST(FixedTimestepTest, InvalidSettingsAreClamped)
{
    const FixedTimestep timestep(0, 0);

    EXPECT_EQ(timestep.getTickRate(), 1);
    EXPECT_EQ(timestep.getMaxTicksPerFrame(), 1);
}

TEST(FixedTimestepTest, OneTickDurationGivesOneTick)
{
    FixedTimestep timestep(60);

    EXPECT_EQ(timestep.consume(1.f / 60.f), 1);
}

TEST(FixedTimestepTest, LeftoverTimeIsKeptForTheNextFrame)
{
    FixedTimestep timestep(60);
    const float halfTick = 0.5f / 60.f;

    EXPECT_EQ(timestep.consume(halfTick), 0);
    EXPECT_NEAR(timestep.getAlpha(), 0.5f, 1e-4f);
    EXPECT_EQ(timestep.consume(halfTick), 1);
    EXPECT_NEAR(timestep.getAlpha(), 0.f, 1e-4f);
}

TEST(FixedTimestepTest, ALongFrameRunsSeveralTicks)
{
    FixedTimestep timestep(60);

    EXPECT_EQ(timestep.consume(3.f / 60.f), 3);
}

TEST(FixedTimestepTest, NegativeTimeIsIgnored)
{
    FixedTimestep timestep(60);

    EXPECT_EQ(timestep.consume(-1.f), 0);
    EXPECT_FLOAT_EQ(timestep.getAlpha(), 0.f);
    EXPECT_EQ(timestep.consume(1.f / 60.f), 1);
}

TEST(FixedTimestepTest, AFreezeDoesNotMakeTheGameFastForward)
{
    FixedTimestep timestep(60, 5);

    // A 2 s freeze would be 120 ticks: only 5 run, the rest is dropped.
    EXPECT_EQ(timestep.consume(2.f), 5);
    EXPECT_FLOAT_EQ(timestep.getAlpha(), 0.f);
    EXPECT_EQ(timestep.consume(1.f / 60.f), 1);
}

TEST(FixedTimestepTest, ResetDropsTheAccumulatedTime)
{
    FixedTimestep timestep(60);
    (void)timestep.consume(0.9f / 60.f);

    timestep.reset();

    EXPECT_FLOAT_EQ(timestep.getAlpha(), 0.f);
    EXPECT_EQ(timestep.consume(0.5f / 60.f), 0);
}

// Whatever the display rate, one second of real time is tickRate ticks.
class FixedTimestepFrameRateTest : public ::testing::TestWithParam<int>
{
};

TEST_P(FixedTimestepFrameRateTest, OneSecondIsAlwaysTickRateTicks)
{
    const int framesPerSecond = GetParam();
    FixedTimestep timestep(60);

    int ticks = 0;
    for (int frame = 0; frame < framesPerSecond; ++frame)
        ticks += timestep.consume(1.f / static_cast<float>(framesPerSecond));

    EXPECT_EQ(ticks, 60);
}

INSTANTIATE_TEST_SUITE_P(FrameRates, FixedTimestepFrameRateTest, ::testing::Values(24, 30, 59, 60, 75, 120, 144, 240));

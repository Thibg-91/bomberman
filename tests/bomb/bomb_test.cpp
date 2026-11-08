#include <gtest/gtest.h>

#include "bomb.h"

using Bomberman::Bomb;
using Bomberman::Position;

TEST(BombTest, StartsWithFullFuseAndHasNotExploded)
{
    const Bomb bomb({2, 3}, 2.f, 1);

    EXPECT_EQ(bomb.getPosition(), (Position{2, 3}));
    EXPECT_FLOAT_EQ(bomb.getFuseDuration(), 2.f);
    EXPECT_FLOAT_EQ(bomb.getTimeRemaining(), 2.f);
    EXPECT_EQ(bomb.getBlastRadius(), 1);
    EXPECT_FALSE(bomb.hasExploded());
}

TEST(BombTest, DefaultsMatchTheSpec)
{
    const Bomb bomb({0, 0});

    EXPECT_FLOAT_EQ(bomb.getFuseDuration(), Bomb::DefaultFuseDuration);
    EXPECT_EQ(bomb.getBlastRadius(), Bomb::DefaultBlastRadius);
}

TEST(BombTest, UpdateCountsDownAndExplodesAfterTheFuseDuration)
{
    Bomb bomb({0, 0}, 2.f, 1);

    bomb.update(1.f);
    EXPECT_FALSE(bomb.hasExploded());
    EXPECT_FLOAT_EQ(bomb.getTimeRemaining(), 1.f);

    bomb.update(1.f);
    EXPECT_TRUE(bomb.hasExploded());
    EXPECT_FLOAT_EQ(bomb.getTimeRemaining(), 0.f);
}

TEST(BombTest, UpdateStopsOnceExploded)
{
    Bomb bomb({0, 0}, 1.f, 1);
    bomb.update(5.f);
    ASSERT_TRUE(bomb.hasExploded());

    bomb.update(1.f);
    EXPECT_FLOAT_EQ(bomb.getTimeRemaining(), 0.f);
}

TEST(BombTest, NegativeDeltaIsIgnored)
{
    Bomb bomb({0, 0}, 2.f, 1);
    bomb.update(-5.f);
    EXPECT_FLOAT_EQ(bomb.getTimeRemaining(), 2.f);
}

TEST(BombTest, SetPositionMovesTheBomb)
{
    Bomb bomb({0, 0});
    bomb.setPosition({4, 4});
    EXPECT_EQ(bomb.getPosition(), (Position{4, 4}));
}

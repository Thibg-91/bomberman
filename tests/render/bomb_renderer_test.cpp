#include <gtest/gtest.h>

#include "bomb_renderer.h"

using Bomberman::Bomb;
using Bomberman::BombRenderer;

// These tests only inspect the generated geometry: no window nor OpenGL
// context is needed, so they run on headless machines (CI).

TEST(BombRendererTest, IsCenteredOnTheBombCell)
{
    BombRenderer renderer(32.f);
    const Bomb bomb({2, 1});
    renderer.update(bomb);

    EXPECT_FLOAT_EQ(renderer.getCenter().x, 2.5f * 32.f);
    EXPECT_FLOAT_EQ(renderer.getCenter().y, 1.5f * 32.f);
    EXPECT_EQ(renderer.getBody().getPosition(), renderer.getCenter());
}

TEST(BombRendererTest, SparkIsFullyVisibleRightAfterTheBombIsDropped)
{
    BombRenderer renderer(32.f);
    const Bomb bomb({0, 0}, 2.f);
    renderer.update(bomb);

    EXPECT_EQ(renderer.getSpark().getFillColor().a, 255);
}

namespace
{

// Number of times the spark's visibility flips while sampling a bomb's fuse
// in small steps over a fixed time window.
int countBlinkToggles(Bomb bomb, BombRenderer renderer, float sampleStep, float windowDuration)
{
    renderer.update(bomb);
    bool previouslyVisible = renderer.getSpark().getFillColor().a > 127;

    int toggles = 0;
    for (float t = 0.f; t < windowDuration; t += sampleStep)
    {
        bomb.update(sampleStep);
        renderer.update(bomb);
        const bool visible = renderer.getSpark().getFillColor().a > 127;
        if (visible != previouslyVisible)
            ++toggles;
        previouslyVisible = visible;
    }
    return toggles;
}

} // namespace

TEST(BombRendererTest, SparkBlinksFasterAsTheFuseRunsOut)
{
    constexpr float SampleStep = 0.01f;
    constexpr float WindowDuration = 0.3f;

    // Same window length, sampled right after lighting the fuse versus right
    // before it explodes: the spark should toggle visible/hidden more often
    // close to the explosion.
    const int togglesWhenFresh = countBlinkToggles(Bomb({0, 0}, 2.f), BombRenderer(32.f), SampleStep, WindowDuration);

    Bomb almostExploding({0, 0}, 2.f);
    almostExploding.update(2.f - WindowDuration);
    const int togglesNearExplosion =
        countBlinkToggles(almostExploding, BombRenderer(32.f), SampleStep, WindowDuration);

    EXPECT_GT(togglesNearExplosion, togglesWhenFresh);
}

TEST(BombRendererTest, BodyPulsesAroundItsRestScale)
{
    BombRenderer renderer(32.f);
    Bomb bomb({0, 0}, 2.f);

    renderer.update(bomb);
    const float initialScale = renderer.getBody().getScale().x;
    EXPECT_NEAR(initialScale, 1.f, 0.2f);

    bomb.update(0.3f);
    renderer.update(bomb);
    EXPECT_GE(renderer.getBody().getScale().x, 0.8f);
    EXPECT_LE(renderer.getBody().getScale().x, 1.2f);
}

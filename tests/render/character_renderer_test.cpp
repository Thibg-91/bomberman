#include <gtest/gtest.h>

#include "character_renderer.h"

using Bomberman::Character;
using Bomberman::CharacterRenderer;
using Bomberman::Direction;
using Bomberman::Grid;

TEST(CharacterRendererTest, IdleCharacterIsCenteredOnItsCell)
{
    CharacterRenderer renderer(10.f);
    renderer.update(Character({2, 1}));

    EXPECT_EQ(renderer.getCenter(), sf::Vector2f(25.f, 15.f));
}

TEST(CharacterRendererTest, MovingCharacterSlidesBetweenCells)
{
    const Grid grid(5, 5);
    Character character({1, 1}, 1.f);
    ASSERT_TRUE(character.move(Direction::Right, grid));

    CharacterRenderer renderer(10.f);
    renderer.update(character);
    EXPECT_EQ(renderer.getCenter(), sf::Vector2f(15.f, 15.f));

    character.update(0.5f);
    renderer.update(character);
    EXPECT_EQ(renderer.getCenter(), sf::Vector2f(20.f, 15.f));

    character.update(0.5f);
    renderer.update(character);
    EXPECT_EQ(renderer.getCenter(), sf::Vector2f(25.f, 15.f));
}

TEST(CharacterRendererTest, WalkingMakesTheBodyHop)
{
    const Grid grid(5, 5);
    Character character({1, 1}, 1.f);
    ASSERT_TRUE(character.move(Direction::Down, grid));
    character.update(0.5f);

    CharacterRenderer renderer(10.f);
    renderer.update(character);

    // Halfway through the step the body is lifted above its sliding center.
    EXPECT_LT(renderer.getBody().getPosition().y, renderer.getCenter().y);
}

TEST(CharacterRendererTest, StartsWithoutPlayingTheBombAnimation)
{
    CharacterRenderer renderer(10.f);
    renderer.update(Character({0, 0}));

    EXPECT_FALSE(renderer.isPlayingBombAnimation());
}

TEST(CharacterRendererTest, TriggerBombAnimationStartsAndEndsTheBombDropPose)
{
    CharacterRenderer renderer(10.f);
    const Character character({0, 0});
    renderer.update(character);

    renderer.triggerBombAnimation();
    EXPECT_TRUE(renderer.isPlayingBombAnimation());

    renderer.update(character, CharacterRenderer::BombAnimDuration);
    EXPECT_FALSE(renderer.isPlayingBombAnimation());
}

TEST(CharacterRendererTest, BombAnimationDoesNotProgressWithoutDeltaTime)
{
    CharacterRenderer renderer(10.f);
    const Character character({0, 0});

    renderer.triggerBombAnimation();
    renderer.update(character); // deltaSeconds defaults to 0

    EXPECT_TRUE(renderer.isPlayingBombAnimation());
}

TEST(CharacterRendererTest, PaletteForPlayerGivesFourDistinctBandanaColors)
{
    const auto p0 = CharacterRenderer::paletteForPlayer(0);
    const auto p1 = CharacterRenderer::paletteForPlayer(1);
    const auto p2 = CharacterRenderer::paletteForPlayer(2);
    const auto p3 = CharacterRenderer::paletteForPlayer(3);

    EXPECT_NE(p0.bandana, p1.bandana);
    EXPECT_NE(p0.bandana, p2.bandana);
    EXPECT_NE(p0.bandana, p3.bandana);
    EXPECT_NE(p1.bandana, p2.bandana);
    EXPECT_NE(p1.bandana, p3.bandana);
    EXPECT_NE(p2.bandana, p3.bandana);

    // Skin stays the same for every crewmate.
    EXPECT_EQ(p0.skin, p1.skin);
    EXPECT_EQ(p0.skin, p2.skin);
    EXPECT_EQ(p0.skin, p3.skin);
}

TEST(CharacterRendererTest, PaletteForPlayerClampsOutOfRangeIndices)
{
    EXPECT_EQ(CharacterRenderer::paletteForPlayer(-1).bandana, CharacterRenderer::paletteForPlayer(0).bandana);
    EXPECT_EQ(CharacterRenderer::paletteForPlayer(99).bandana,
              CharacterRenderer::paletteForPlayer(CharacterRenderer::PlayerCount - 1).bandana);
}

TEST(CharacterRendererTest, EveryFacingKeepsTheBodyCenteredOnTheCell)
{
    CharacterRenderer renderer(10.f);
    const Grid grid(5, 5);
    Character character({2, 2}, 0.f); // moveDuration 0: always idle right after a move

    for (Direction facing : {Direction::Down, Direction::Up, Direction::Left, Direction::Right})
    {
        ASSERT_TRUE(character.move(facing, grid));
        renderer.update(character);

        // The artwork changes per facing, but the body itself never moves
        // off the (idle, hop-less) cell center.
        EXPECT_EQ(renderer.getBody().getPosition(), renderer.getCenter());
    }
}

TEST(CharacterRendererTest, BombAnimationStillRunsWhateverTheFacing)
{
    CharacterRenderer renderer(10.f);
    const Grid grid(5, 5);
    Character character({2, 2}, 0.f);
    ASSERT_TRUE(character.move(Direction::Left, grid));

    renderer.triggerBombAnimation();
    renderer.update(character, 0.1f);

    EXPECT_TRUE(renderer.isPlayingBombAnimation());
}

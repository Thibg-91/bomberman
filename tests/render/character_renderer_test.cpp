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

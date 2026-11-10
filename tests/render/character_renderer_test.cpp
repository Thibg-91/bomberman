#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "character_renderer.h"

using Bomberman::Character;
using Bomberman::CharacterRenderer;
using Bomberman::Direction;
using Bomberman::Grid;

namespace
{

// An idle character facing direction: a move towards a new direction only
// turns it (and every move is blocked on a 1x1 grid anyway).
Character facing(Direction direction)
{
    Character character({0, 0}, 0.f);
    character.move(direction, Grid(1, 1));
    return character;
}

sf::FloatRect bounds(const sf::VertexArray& vertices)
{
    sf::Vector2f min = vertices[0].position;
    sf::Vector2f max = min;
    for (std::size_t i = 1; i < vertices.getVertexCount(); ++i)
    {
        min.x = std::min(min.x, vertices[i].position.x);
        min.y = std::min(min.y, vertices[i].position.y);
        max.x = std::max(max.x, vertices[i].position.x);
        max.y = std::max(max.y, vertices[i].position.y);
    }
    return {min, max - min};
}

bool usesColor(const sf::VertexArray& vertices, sf::Color color)
{
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
        if (vertices[i].color == color)
            return true;
    return false;
}

} // namespace

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
    ASSERT_TRUE(character.move(Direction::Right, grid)); // moves at once

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
    EXPECT_LT(renderer.getBodyCenter().y, renderer.getCenter().y);
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
        character.move(facing, grid); // turns (no-op when already facing it)
        ASSERT_EQ(character.getFacing(), facing);
        renderer.update(character);

        // The artwork changes per facing, but the body itself never moves
        // off the (idle, hop-less) cell center.
        EXPECT_EQ(renderer.getBodyCenter(), renderer.getCenter());
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

// The geometry is only inspected, so no window nor OpenGL context is needed.
TEST(CharacterRendererTest, BuildsTriangles)
{
    CharacterRenderer renderer(40.f);
    renderer.update(facing(Direction::Down));

    EXPECT_EQ(renderer.getVertices().getPrimitiveType(), sf::PrimitiveType::Triangles);
    EXPECT_GT(renderer.getVertices().getVertexCount(), 0u);
    EXPECT_EQ(renderer.getVertices().getVertexCount() % 3u, 0u);
}

TEST(CharacterRendererTest, StaysWithinItsCell)
{
    CharacterRenderer renderer(40.f);
    for (Direction direction : {Direction::Down, Direction::Up, Direction::Left, Direction::Right})
    {
        renderer.update(facing(direction));
        const sf::FloatRect box = bounds(renderer.getVertices());

        EXPECT_GE(box.position.x, 0.f);
        EXPECT_GE(box.position.y, 0.f);
        EXPECT_LE(box.position.x + box.size.x, 40.f);
        EXPECT_LE(box.position.y + box.size.y, 40.f);
    }
}

TEST(CharacterRendererTest, BackViewHasNoFaceSoItDiffersFromTheFront)
{
    CharacterRenderer front(40.f);
    CharacterRenderer back(40.f);
    front.update(facing(Direction::Down));
    back.update(facing(Direction::Up));

    EXPECT_NE(front.getVertices().getVertexCount(), back.getVertices().getVertexCount());
}

TEST(CharacterRendererTest, RightProfileMirrorsLeftProfile)
{
    CharacterRenderer left(40.f);
    CharacterRenderer right(40.f);
    left.update(facing(Direction::Left));
    right.update(facing(Direction::Right));

    ASSERT_EQ(left.getVertices().getVertexCount(), right.getVertices().getVertexCount());
    const sf::FloatRect l = bounds(left.getVertices());
    const sf::FloatRect r = bounds(right.getVertices());
    const float centerX = left.getBodyCenter().x;
    // Circles are polygons whose vertex 0 sits at angle 0, so their extreme
    // left and right points differ by a fraction of a pixel once mirrored.
    constexpr float Tessellation = 0.5f;
    EXPECT_NEAR(r.position.x, 2.f * centerX - (l.position.x + l.size.x), Tessellation);
    EXPECT_NEAR(r.size.x, l.size.x, Tessellation);
    EXPECT_NEAR(r.position.y, l.position.y, Tessellation);
    EXPECT_NEAR(r.size.y, l.size.y, Tessellation);
}

TEST(CharacterRendererTest, BombPoseChangesTheArtworkThenGoesBack)
{
    const Character character = facing(Direction::Down);
    CharacterRenderer idle(40.f);
    idle.update(character);

    CharacterRenderer posing(40.f);
    posing.triggerBombAnimation();
    posing.update(character, CharacterRenderer::BombAnimDuration / 2.f);
    EXPECT_NE(posing.getVertices().getVertexCount(), idle.getVertices().getVertexCount());

    posing.update(character, CharacterRenderer::BombAnimDuration);
    ASSERT_EQ(posing.getVertices().getVertexCount(), idle.getVertices().getVertexCount());
    for (std::size_t i = 0; i < idle.getVertices().getVertexCount(); ++i)
        ASSERT_EQ(posing.getVertices()[i].position, idle.getVertices()[i].position);
}

TEST(CharacterRendererTest, BandanaUsesThePaletteColor)
{
    const CharacterRenderer::Palette blue = CharacterRenderer::paletteForPlayer(1);
    CharacterRenderer renderer(40.f, blue);
    renderer.update(facing(Direction::Down));

    EXPECT_TRUE(usesColor(renderer.getVertices(), blue.bandana));
    EXPECT_TRUE(usesColor(renderer.getVertices(), blue.skin));
    EXPECT_FALSE(usesColor(renderer.getVertices(), CharacterRenderer::paletteForPlayer(0).bandana));
}

TEST(CharacterRendererTest, TheBombSparkLightsUpUnderTheCharacter)
{
    // The bomb is dropped on the character's own cell: the spark (drawn
    // first) is centered under it, whatever it faces.
    const Grid grid(5, 5);
    for (const Direction direction : {Direction::Left, Direction::Up})
    {
        Character character({2, 2}, 0.f);
        character.move(direction, grid);
        CharacterRenderer renderer(10.f);
        renderer.triggerBombAnimation();
        renderer.update(character, 0.2f);

        float minX = 1e9f;
        float maxX = -1e9f;
        const sf::VertexArray& vertices = renderer.getVertices();
        for (std::size_t i = 0; i < vertices.getVertexCount() && vertices[i].color.r == 255; ++i)
        {
            minX = std::min(minX, vertices[i].position.x);
            maxX = std::max(maxX, vertices[i].position.x);
        }
        EXPECT_NEAR((minX + maxX) / 2.f, renderer.getCenter().x, 0.05f);
    }
}

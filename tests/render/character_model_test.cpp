#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

#include <gtest/gtest.h>

#include "character_renderer.h"

using Bomberman::AllCharacterModels;
using Bomberman::Character;
using Bomberman::CharacterModel;
using Bomberman::CharacterRenderer;
using Bomberman::Direction;
using Bomberman::Grid;

namespace
{

constexpr float TileSize = 100.f;

Character facing(Direction direction)
{
    Character character({0, 0}, 0.f);
    character.move(direction, Grid(1, 1));
    return character;
}

sf::VertexArray render(CharacterModel model, Direction direction, float brightness = 1.f)
{
    CharacterRenderer renderer(TileSize, CharacterRenderer::Palette{}, model);
    renderer.setBrightness(brightness);
    renderer.update(facing(direction));
    return renderer.getVertices();
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

const sf::Color Skin(219, 163, 107);
const sf::Color Hat(42, 42, 51);
const sf::Color Knit(47, 111, 174);
const sf::Color Feathers(63, 163, 77);
const sf::Color Beak(242, 201, 76);
const sf::Color Skeleton(236, 230, 214);
const sf::Color Glow(123, 224, 160);

const Direction AllDirections[] = {Direction::Down, Direction::Up, Direction::Left, Direction::Right};

class ModelViewTest : public ::testing::TestWithParam<std::tuple<CharacterModel, Direction>>
{
};

} // namespace

TEST(CharacterModelTest, TheBuccaneerIsTheDefault)
{
    const CharacterRenderer renderer(TileSize);
    EXPECT_EQ(renderer.getModel(), CharacterModel::Buccaneer);
    EXPECT_FLOAT_EQ(renderer.getBrightness(), 1.f);

    // Same drawing as an explicit Buccaneer.
    CharacterRenderer byDefault(TileSize);
    byDefault.update(facing(Direction::Down));
    const sf::VertexArray explicitOne = render(CharacterModel::Buccaneer, Direction::Down);
    ASSERT_EQ(byDefault.getVertices().getVertexCount(), explicitOne.getVertexCount());
    for (std::size_t i = 0; i < explicitOne.getVertexCount(); ++i)
        EXPECT_EQ(byDefault.getVertices()[i].position, explicitOne[i].position);
}

TEST(CharacterModelTest, SixModelsInSelectionOrder)
{
    EXPECT_EQ(Bomberman::CharacterModelCount, 6);
    EXPECT_EQ(AllCharacterModels.front(), CharacterModel::Buccaneer);
    EXPECT_EQ(AllCharacterModels.back(), CharacterModel::Spaniel);
    EXPECT_EQ(std::set<CharacterModel>(AllCharacterModels.begin(), AllCharacterModels.end()).size(), 6u);
}

TEST_P(ModelViewTest, IsDrawnAroundItsCell)
{
    const auto [model, direction] = GetParam();
    const sf::VertexArray vertices = render(model, direction);
    ASSERT_GT(vertices.getVertexCount(), 0u);

    // Centered on the cell (50, 50); heads, hats and tails may stick out of
    // the tile a little, but stay within half a tile around it.
    const sf::FloatRect box = bounds(vertices);
    EXPECT_GE(box.position.x, -50.f);
    EXPECT_GE(box.position.y, -50.f);
    EXPECT_LE(box.position.x + box.size.x, 150.f);
    EXPECT_LE(box.position.y + box.size.y, 150.f);
}

TEST_P(ModelViewTest, BrightnessDimsEveryColorButKeepsTheShape)
{
    const auto [model, direction] = GetParam();
    const sf::VertexArray normal = render(model, direction);
    const sf::VertexArray dimmed = render(model, direction, 0.6f);

    ASSERT_EQ(normal.getVertexCount(), dimmed.getVertexCount());
    for (std::size_t i = 0; i < normal.getVertexCount(); ++i)
    {
        EXPECT_EQ(normal[i].position, dimmed[i].position);
        EXPECT_NEAR(dimmed[i].color.r, std::lround(normal[i].color.r * 0.6f), 1);
        EXPECT_NEAR(dimmed[i].color.g, std::lround(normal[i].color.g * 0.6f), 1);
        EXPECT_NEAR(dimmed[i].color.b, std::lround(normal[i].color.b * 0.6f), 1);
        EXPECT_EQ(dimmed[i].color.a, normal[i].color.a);
    }
}

TEST_P(ModelViewTest, TheRightProfileMirrorsTheLeftOne)
{
    const auto [model, direction] = GetParam();
    if (direction != Direction::Left)
        GTEST_SKIP() << "only checked once per model";
    const sf::VertexArray left = render(model, Direction::Left);
    const sf::VertexArray right = render(model, Direction::Right);

    ASSERT_EQ(left.getVertexCount(), right.getVertexCount());
    const sf::FloatRect l = bounds(left);
    const sf::FloatRect r = bounds(right);
    // Mirrored around the cell's vertical axis (x = 50). Round joins are
    // tessellated from angle 0 on both sides, hence a tiny tolerance (the
    // spaniel's pointy snout tip is off by 0.11).
    EXPECT_NEAR(l.position.x, 100.f - (r.position.x + r.size.x), 0.15f);
    EXPECT_NEAR(l.size.y, r.size.y, 0.1f);
}

INSTANTIATE_TEST_SUITE_P(AllModelsAllViews, ModelViewTest,
                         ::testing::Combine(::testing::ValuesIn(AllCharacterModels),
                                            ::testing::ValuesIn(AllDirections)));

TEST(CharacterModelTest, EachModelHasItsOwnLook)
{
    const sf::VertexArray captain = render(CharacterModel::Captain, Direction::Down);
    const sf::VertexArray lookout = render(CharacterModel::Lookout, Direction::Down);
    const sf::VertexArray parrot = render(CharacterModel::Parrot, Direction::Down);
    const sf::VertexArray revenant = render(CharacterModel::Revenant, Direction::Down);

    EXPECT_TRUE(usesColor(captain, Hat));
    EXPECT_TRUE(usesColor(captain, Skin));
    EXPECT_TRUE(usesColor(lookout, Knit));
    EXPECT_TRUE(usesColor(lookout, Skin));
    EXPECT_TRUE(usesColor(parrot, Feathers));
    EXPECT_TRUE(usesColor(parrot, Beak));
    EXPECT_FALSE(usesColor(parrot, Skin));
    EXPECT_TRUE(usesColor(revenant, Skeleton));
    EXPECT_TRUE(usesColor(revenant, Glow));
    EXPECT_FALSE(usesColor(revenant, Skin));
}

TEST(CharacterModelTest, TheBackHidesTheFace)
{
    // No eye highlight, beak or glowing sockets seen from behind.
    EXPECT_FALSE(usesColor(render(CharacterModel::Parrot, Direction::Up), Beak));
    EXPECT_FALSE(usesColor(render(CharacterModel::Revenant, Direction::Up), Glow));
    EXPECT_FALSE(usesColor(render(CharacterModel::Captain, Direction::Up), sf::Color::White));
}

TEST(CharacterModelTest, ThePaletteSkinAppliesToHumansOnly)
{
    const CharacterRenderer::Palette pale(sf::Color(250, 220, 200), sf::Color(195, 59, 59));
    for (const CharacterModel model : {CharacterModel::Captain, CharacterModel::Lookout})
    {
        CharacterRenderer renderer(TileSize, pale, model);
        renderer.update(facing(Direction::Down));
        EXPECT_TRUE(usesColor(renderer.getVertices(), pale.skin));
    }
    CharacterRenderer parrot(TileSize, pale, CharacterModel::Parrot);
    parrot.update(facing(Direction::Down));
    EXPECT_FALSE(usesColor(parrot.getVertices(), pale.skin));
}

TEST(CharacterModelTest, SetModelAppliesAtTheNextUpdate)
{
    CharacterRenderer renderer(TileSize);
    renderer.update(facing(Direction::Down));
    EXPECT_FALSE(usesColor(renderer.getVertices(), Feathers));

    renderer.setModel(CharacterModel::Parrot);
    EXPECT_EQ(renderer.getModel(), CharacterModel::Parrot);
    renderer.update(facing(Direction::Down));
    EXPECT_TRUE(usesColor(renderer.getVertices(), Feathers));
}

TEST(CharacterModelTest, BrightnessIsClamped)
{
    CharacterRenderer renderer(TileSize);
    renderer.setBrightness(2.f);
    EXPECT_FLOAT_EQ(renderer.getBrightness(), 1.f);
    renderer.setBrightness(-1.f);
    EXPECT_FLOAT_EQ(renderer.getBrightness(), 0.f);
}

TEST(CharacterModelTest, ThinStrokesAreOpaque)
{
    // Thin strokes overlap themselves (segments + round joins): a translucent
    // color would draw darker dots. Every non-ellipse translucent color must
    // stay out of the new models' strokes.
    for (const CharacterModel model : {CharacterModel::Lookout, CharacterModel::Parrot, CharacterModel::Revenant})
        for (const Direction direction : AllDirections)
        {
            const sf::VertexArray vertices = render(model, direction);
            for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
            {
                const sf::Color c = vertices[i].color;
                const bool allowedTranslucent = c == sf::Color(240, 132, 138, 140)    // blush (one ellipse)
                                             || c == sf::Color(201, 138, 90, 179)     // inner ear (one ellipse)
                                             || c == sf::Color(236, 230, 214, 217);   // bandana dots (ellipses)
                EXPECT_TRUE(c.a == 255 || allowedTranslucent) << "color " << int(c.r) << "," << int(c.g) << ","
                                                              << int(c.b) << "," << int(c.a);
                if (c.a != 255 && !allowedTranslucent)
                    return;
            }
        }
}

TEST(CharacterModelTest, TheBombPoseWorksForEveryModel)
{
    for (const CharacterModel model : AllCharacterModels)
    {
        CharacterRenderer renderer(TileSize, CharacterRenderer::Palette{}, model);
        renderer.update(facing(Direction::Down));
        const sf::VertexArray idle = renderer.getVertices();

        renderer.triggerBombAnimation();
        renderer.update(facing(Direction::Down), CharacterRenderer::BombAnimDuration / 2.f);
        const sf::VertexArray pose = renderer.getVertices();

        EXPECT_TRUE(renderer.isPlayingBombAnimation());
        // The ground spark is added and the arms are raised: the drawing
        // changes, the head and face stay the same.
        EXPECT_GT(pose.getVertexCount(), idle.getVertexCount());
    }
}

// =========================
// Spaniel (validated mock-up "Le chien du bord", proposal A)
// =========================
namespace
{

constexpr sf::Color Chocolate(110, 59, 30);
constexpr sf::Color Tan(217, 143, 69);
constexpr sf::Color Neckerchief(195, 59, 59);
constexpr sf::Color DogNose(42, 22, 16);
constexpr sf::Color Tongue(232, 115, 122);

sf::VertexArray renderSpanielAt(Direction direction, float seconds)
{
    CharacterRenderer renderer(TileSize, CharacterRenderer::Palette{}, CharacterModel::Spaniel);
    renderer.update(facing(direction), seconds);
    return renderer.getVertices();
}

bool sameVertices(const sf::VertexArray& a, const sf::VertexArray& b)
{
    if (a.getVertexCount() != b.getVertexCount())
        return false;
    for (std::size_t i = 0; i < a.getVertexCount(); ++i)
        if (a[i].position != b[i].position)
            return false;
    return true;
}

} // namespace

TEST(SpanielTest, AChocolateAndTanCockerWithARedNeckerchief)
{
    const sf::VertexArray front = render(CharacterModel::Spaniel, Direction::Down);
    EXPECT_TRUE(usesColor(front, Chocolate));
    EXPECT_TRUE(usesColor(front, Tan));
    EXPECT_TRUE(usesColor(front, Neckerchief));
    EXPECT_TRUE(usesColor(front, DogNose));
    EXPECT_TRUE(usesColor(front, Tongue));
    EXPECT_FALSE(usesColor(front, Skin)); // the palette's skin is for the human models
}

TEST(SpanielTest, TheBackHidesTheMuzzle)
{
    const sf::VertexArray back = render(CharacterModel::Spaniel, Direction::Up);
    EXPECT_FALSE(usesColor(back, DogNose));
    EXPECT_FALSE(usesColor(back, Tongue));
    EXPECT_TRUE(usesColor(back, Neckerchief)); // its knot
    EXPECT_TRUE(usesColor(render(CharacterModel::Spaniel, Direction::Left), DogNose));
}

TEST(SpanielTest, ItsTailWagsEvenStandingStill)
{
    // A quarter of a wag apart: the tail has moved (back and profiles).
    for (const Direction direction : {Direction::Up, Direction::Left, Direction::Right})
        EXPECT_FALSE(sameVertices(renderSpanielAt(direction, 0.f), renderSpanielAt(direction, 0.125f)));
    // Seen from the front, there is no tail.
    EXPECT_TRUE(sameVertices(renderSpanielAt(Direction::Down, 0.f), renderSpanielAt(Direction::Down, 0.125f)));
}

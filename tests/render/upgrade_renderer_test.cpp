#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "upgrade_renderer.h"

using Bomberman::Position;
using Bomberman::Upgrade;
using Bomberman::UpgradeRenderer;
using Bomberman::UpgradeType;

namespace
{

constexpr float TileSize = 100.f; // 1 mock-up unit = 1 pixel

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

sf::VertexArray render(UpgradeType type, Position position = {0, 0}, float deltaSeconds = 0.f)
{
    UpgradeRenderer renderer(TileSize);
    renderer.update({Upgrade{position, type}}, deltaSeconds);
    return renderer.getVertices();
}

// Signature color of each design (mock-up fills).
const sf::Color BombBody(45, 45, 53);
const sf::Color Skin(219, 163, 107);
const sf::Color BurstOuter(242, 145, 58);

class UpgradeRendererTypeTest : public ::testing::TestWithParam<UpgradeType>
{
};

} // namespace

TEST(UpgradeRendererTest, NothingToDrawWithoutUpgrades)
{
    UpgradeRenderer renderer(TileSize);
    renderer.update({});

    EXPECT_EQ(renderer.getVertices().getVertexCount(), 0u);
    EXPECT_EQ(renderer.getVertices().getPrimitiveType(), sf::PrimitiveType::Triangles);
    EXPECT_FLOAT_EQ(renderer.getTileSize(), TileSize);
}

TEST_P(UpgradeRendererTypeTest, StaysInsideItsCellWhileBobbing)
{
    // Rest, half way and top of the bob.
    for (const float t : {0.f, 0.4f, 0.8f, 1.2f})
    {
        const sf::FloatRect box = bounds(render(GetParam(), {2, 3}, t));
        EXPECT_GE(box.position.x, 200.f) << "t=" << t;
        EXPECT_GE(box.position.y, 300.f) << "t=" << t;
        EXPECT_LE(box.position.x + box.size.x, 300.f) << "t=" << t;
        EXPECT_LE(box.position.y + box.size.y, 400.f) << "t=" << t;
    }
}

TEST_P(UpgradeRendererTypeTest, DrawsAShadowOnTheFloor)
{
    EXPECT_TRUE(usesColor(render(GetParam()), sf::Color(0, 0, 0, 77)));
}

TEST_P(UpgradeRendererTypeTest, AnotherCellOnlyShiftsTheDrawing)
{
    const sf::VertexArray here = render(GetParam(), {0, 0});
    const sf::VertexArray there = render(GetParam(), {3, 1});

    ASSERT_EQ(here.getVertexCount(), there.getVertexCount());
    for (std::size_t i = 0; i < here.getVertexCount(); ++i)
    {
        EXPECT_NEAR(there[i].position.x - here[i].position.x, 300.f, 1e-3f);
        EXPECT_NEAR(there[i].position.y - here[i].position.y, 100.f, 1e-3f);
    }
}

INSTANTIATE_TEST_SUITE_P(AllTypes, UpgradeRendererTypeTest,
                         ::testing::Values(UpgradeType::ExtraBomb, UpgradeType::PushStrength,
                                           UpgradeType::BlastRadius));

TEST(UpgradeRendererTest, EachUpgradeHasItsOwnLook)
{
    const sf::VertexArray bomb = render(UpgradeType::ExtraBomb);
    const sf::VertexArray arm = render(UpgradeType::PushStrength);
    const sf::VertexArray burst = render(UpgradeType::BlastRadius);

    EXPECT_TRUE(usesColor(bomb, BombBody));
    EXPECT_FALSE(usesColor(bomb, Skin));
    EXPECT_TRUE(usesColor(arm, Skin));
    EXPECT_FALSE(usesColor(arm, BurstOuter));
    EXPECT_TRUE(usesColor(burst, BurstOuter));
    EXPECT_FALSE(usesColor(burst, BombBody));
}

TEST(UpgradeRendererTest, MatchesTheMockUpPlacement)
{
    // At rest the item group is translate(9 5) scale(.82): the bomb body
    // (circle (46, 55) r 21, stroke 3) spans x in 9 + .82 * [23.5, 68.5].
    const sf::VertexArray bomb = render(UpgradeType::ExtraBomb);
    float minX = 1e9f;
    for (std::size_t i = 0; i < bomb.getVertexCount(); ++i)
        if (bomb[i].color == sf::Color(28, 20, 13)) // the body outline is the left-most ink
            minX = std::min(minX, bomb[i].position.x);

    EXPECT_NEAR(minX, 9.f + 0.82f * 23.5f, 0.5f);
}

TEST(UpgradeRendererTest, ItemsBobButTheShadowStays)
{
    UpgradeRenderer renderer(TileSize);
    const std::vector<Upgrade> upgrades = {{{0, 0}, UpgradeType::BlastRadius}};
    renderer.update(upgrades);
    const sf::VertexArray rest = renderer.getVertices();
    EXPECT_FLOAT_EQ(renderer.getBobOffset(), 0.f);

    renderer.update(upgrades, UpgradeRenderer::AnimationPeriod / 2.f); // top of the bob
    const sf::VertexArray top = renderer.getVertices();
    EXPECT_NEAR(renderer.getBobOffset(), UpgradeRenderer::BobAmplitude, 1e-3f);

    ASSERT_EQ(rest.getVertexCount(), top.getVertexCount());
    // The first vertices are the shadow: they do not move.
    EXPECT_EQ(rest[0].position, top[0].position);
    // The burst is lifted by the bob amplitude (in mock units = pixels here).
    const sf::FloatRect restBox = bounds(rest);
    const sf::FloatRect topBox = bounds(top);
    EXPECT_LT(topBox.position.y, restBox.position.y);
}

TEST(UpgradeRendererTest, AnimationLoopsEveryPeriod)
{
    UpgradeRenderer renderer(TileSize);
    const std::vector<Upgrade> upgrades = {{{1, 1}, UpgradeType::ExtraBomb}};
    renderer.update(upgrades, 0.3f);
    const sf::VertexArray before = renderer.getVertices();

    renderer.update(upgrades, UpgradeRenderer::AnimationPeriod);

    EXPECT_NEAR(renderer.getElapsed(), 0.3f, 1e-4f);
    ASSERT_EQ(before.getVertexCount(), renderer.getVertices().getVertexCount());
    for (std::size_t i = 0; i < before.getVertexCount(); ++i)
    {
        EXPECT_NEAR(before[i].position.x, renderer.getVertices()[i].position.x, 1e-3f);
        EXPECT_NEAR(before[i].position.y, renderer.getVertices()[i].position.y, 1e-3f);
    }
}

TEST(UpgradeRendererTest, AllUpgradesShareOneVertexArray)
{
    UpgradeRenderer renderer(TileSize);
    renderer.update({{{0, 0}, UpgradeType::ExtraBomb}});
    const std::size_t one = renderer.getVertices().getVertexCount();

    renderer.update({{{0, 0}, UpgradeType::ExtraBomb}, {{4, 2}, UpgradeType::ExtraBomb}});

    EXPECT_EQ(renderer.getVertices().getVertexCount(), 2u * one);
}

TEST(UpgradeRendererTest, ScalesWithTheTileSize)
{
    UpgradeRenderer small(10.f);
    UpgradeRenderer big(100.f);
    small.update({{{0, 0}, UpgradeType::PushStrength}});
    big.update({{{0, 0}, UpgradeType::PushStrength}});

    const sf::FloatRect smallBox = bounds(small.getVertices());
    const sf::FloatRect bigBox = bounds(big.getVertices());
    // Same drawing, ten times bigger (strokes have a 1 px floor, hence the tolerance).
    EXPECT_NEAR(bigBox.size.x / smallBox.size.x, 10.f, 1.f);
}

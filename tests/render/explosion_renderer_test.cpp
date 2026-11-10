#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "explosion_renderer.h"

using Bomberman::Blast;
using Bomberman::ExplosionRenderer;
using Bomberman::Position;

namespace
{

constexpr float TileSize = 100.f; // 1 mock-up unit = 1 pixel
constexpr float Duration = ExplosionRenderer::Duration;

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

// A cross on (5, 5) reaching left/right reach cells, nothing up/down.
Blast horizontal(int reach)
{
    Blast blast;
    blast.center = {5, 5};
    blast.reach = {0, 0, reach, reach};
    return blast;
}

sf::VertexArray renderAt(ExplosionRenderer::Style style, const Blast& blast, float seconds)
{
    ExplosionRenderer renderer(TileSize, style);
    renderer.add(blast);
    renderer.update(seconds);
    return renderer.getVertices();
}

const sf::Color Ink(28, 20, 13);
const sf::Color FlameOuter(242, 145, 58);
const sf::Color FlameRed(216, 67, 47);
const sf::Color FlameCore(255, 243, 196);

class ExplosionStyleTest : public ::testing::TestWithParam<ExplosionRenderer::Style>
{
};

} // namespace

TEST(ExplosionRendererTest, FireballsAreTheDefaultStyle)
{
    const ExplosionRenderer renderer(TileSize);

    EXPECT_EQ(renderer.getStyle(), ExplosionRenderer::Style::Fireballs);
    EXPECT_FLOAT_EQ(renderer.getTileSize(), TileSize);
    EXPECT_EQ(renderer.getActiveCount(), 0u);
}

TEST_P(ExplosionStyleTest, NothingIsDrawnWithoutExplosions)
{
    ExplosionRenderer renderer(TileSize, GetParam());
    renderer.update(0.1f);

    EXPECT_EQ(renderer.getVertices().getVertexCount(), 0u);
    EXPECT_EQ(renderer.getVertices().getPrimitiveType(), sf::PrimitiveType::Triangles);
}

TEST_P(ExplosionStyleTest, AnExplosionLastsDurationSeconds)
{
    ExplosionRenderer renderer(TileSize, GetParam());
    renderer.add(horizontal(2));

    renderer.update(Duration * 0.9f);
    EXPECT_EQ(renderer.getActiveCount(), 1u);
    EXPECT_GT(renderer.getVertices().getVertexCount(), 0u);

    renderer.update(Duration * 0.2f);
    EXPECT_EQ(renderer.getActiveCount(), 0u);
    EXPECT_EQ(renderer.getVertices().getVertexCount(), 0u);
}

TEST_P(ExplosionStyleTest, ItIsOpaqueThenFadesOut)
{
    ExplosionRenderer renderer(TileSize, GetParam());
    renderer.add(horizontal(1));

    renderer.update(Duration * 0.5f);
    EXPECT_FLOAT_EQ(renderer.getOpacity(0), 1.f);

    renderer.update(Duration * 0.3f); // u = 0.8: half way through the fade
    EXPECT_NEAR(renderer.getOpacity(0), 0.5f, 1e-3f);
    EXPECT_THROW((void)renderer.getOpacity(1), std::out_of_range);
}

TEST_P(ExplosionStyleTest, TheCrossFollowsTheBlastRadius)
{
    // Fully grown, the cross spans 2 * reach cells plus the end caps.
    for (int reach = 1; reach <= 4; ++reach)
    {
        const sf::FloatRect box = bounds(renderAt(GetParam(), horizontal(reach), Duration * 0.5f));
        const float centerX = 5.5f * TileSize;
        EXPECT_NEAR(box.position.x + box.size.x / 2.f, centerX, 1.f) << "reach " << reach;
        EXPECT_GT(box.size.x, 2.f * static_cast<float>(reach) * TileSize) << "reach " << reach;
        EXPECT_LT(box.size.x, (2.f * static_cast<float>(reach) + 1.2f) * TileSize) << "reach " << reach;
        // No vertical arm: the cross stays about one cell high.
        EXPECT_LT(box.size.y, 1.2f * TileSize) << "reach " << reach;
    }
}

TEST_P(ExplosionStyleTest, ItStartsFromTheCenter)
{
    // The cross shoots out within the first quarter of the explosion.
    const sf::FloatRect early = bounds(renderAt(GetParam(), horizontal(3), Duration * 0.01f));
    const sf::FloatRect full = bounds(renderAt(GetParam(), horizontal(3), Duration * 0.5f));

    EXPECT_LT(early.size.x, full.size.x / 2.f);
}

TEST_P(ExplosionStyleTest, AnInkOutlineSurroundsTheFire)
{
    EXPECT_TRUE(usesColor(renderAt(GetParam(), horizontal(2), Duration * 0.3f), Ink));
}

TEST_P(ExplosionStyleTest, SeveralExplosionsShareOneVertexArray)
{
    ExplosionRenderer renderer(TileSize, GetParam());
    renderer.add(horizontal(1));
    renderer.update(0.1f);
    const std::size_t one = renderer.getVertices().getVertexCount();

    Blast other = horizontal(1);
    other.center = {2, 2};
    renderer.add(other);
    renderer.update(0.1f);

    EXPECT_EQ(renderer.getActiveCount(), 2u);
    EXPECT_GT(one, 0u);
    // Both crosses are in the array: from cell (1, 2) to cell (6, 5).
    const sf::FloatRect box = bounds(renderer.getVertices());
    EXPECT_LT(box.position.x, 2.f * TileSize);
    EXPECT_GT(box.position.x + box.size.x, 6.f * TileSize);
}

INSTANTIATE_TEST_SUITE_P(Styles, ExplosionStyleTest,
                         ::testing::Values(ExplosionRenderer::Style::Fireballs,
                                           ExplosionRenderer::Style::FlameCross));

TEST(ExplosionRendererTest, FireballsPopCellAfterCell)
{
    // Each cell pops 0.05 of the timeline after the previous one: at 0.1 only
    // the balls up to 2 cells from the center have started.
    const sf::FloatRect early = bounds(renderAt(ExplosionRenderer::Style::Fireballs, horizontal(4), Duration * 0.1f));
    const sf::FloatRect full = bounds(renderAt(ExplosionRenderer::Style::Fireballs, horizontal(4), Duration * 0.5f));

    EXPECT_LT(early.size.x, 4.f * TileSize);
    EXPECT_GT(full.size.x, 8.f * TileSize);
}

TEST(ExplosionRendererTest, FireballsTurnToSmokeAtTheEnd)
{
    const sf::VertexArray fire = renderAt(ExplosionRenderer::Style::Fireballs, horizontal(1), Duration * 0.3f);
    const sf::VertexArray smoke = renderAt(ExplosionRenderer::Style::Fireballs, horizontal(1), Duration * 0.99f);

    EXPECT_TRUE(usesColor(fire, FlameOuter));
    EXPECT_FALSE(usesColor(smoke, FlameOuter));
    // Almost fully turned to grey smoke (r, g, b nearly equal).
    bool grey = false;
    for (std::size_t i = 0; i < smoke.getVertexCount(); ++i)
    {
        const sf::Color c = smoke[i].color;
        grey = grey || (c.r > 100 && std::abs(int(c.r) - int(c.b)) < 20 && std::abs(int(c.r) - int(c.g)) < 20);
    }
    EXPECT_TRUE(grey);
}

TEST(ExplosionRendererTest, TheFlameCrossHasItsFourFlameLayers)
{
    const sf::VertexArray cross = renderAt(ExplosionRenderer::Style::FlameCross, horizontal(2), Duration * 0.3f);

    EXPECT_TRUE(usesColor(cross, FlameRed));
    EXPECT_TRUE(usesColor(cross, FlameOuter));
    EXPECT_TRUE(usesColor(cross, FlameCore));
}

TEST(ExplosionRendererTest, ACenterOnlyBlastStaysOnItsCell)
{
    Blast blast;
    blast.center = {3, 1};
    const sf::FloatRect box = bounds(renderAt(ExplosionRenderer::Style::Fireballs, blast, Duration * 0.5f));

    EXPECT_NEAR(box.position.x + box.size.x / 2.f, 3.5f * TileSize, 1.f);
    EXPECT_LT(box.size.x, 1.2f * TileSize);
}

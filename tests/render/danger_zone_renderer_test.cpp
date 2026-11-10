#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "danger_zone_renderer.h"

using Bomberman::Bomb;
using Bomberman::DangerZoneRenderer;
using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

constexpr float TileSize = 100.f;

// Cells holding at least one tint vertex.
std::vector<Position> tintedCells(const sf::VertexArray& vertices)
{
    std::vector<Position> cells;
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
    {
        const Position cell{static_cast<int>(vertices[i].position.x / TileSize),
                            static_cast<int>(vertices[i].position.y / TileSize)};
        if (std::find(cells.begin(), cells.end(), cell) == cells.end())
            cells.push_back(cell);
    }
    std::sort(cells.begin(), cells.end(), [](Position a, Position b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return cells;
}

} // namespace

TEST(DangerZoneRendererTest, NothingWithoutBombs)
{
    DangerZoneRenderer renderer(TileSize);
    renderer.update({}, Grid(5, 5));

    EXPECT_EQ(renderer.getVertices().getVertexCount(), 0u);
    EXPECT_FLOAT_EQ(renderer.getTileSize(), TileSize);
}

TEST(DangerZoneRendererTest, TintsExactlyTheCellsTheBombWillHit)
{
    Grid grid(7, 7);
    grid.setTile({3, 1}, Tile(TileType::Wall));              // stops the up arm before it
    grid.setTile({5, 3}, Tile(TileType::DestructibleBlock)); // hit, stops the right arm
    const Bomb bomb({3, 3}, Bomb::DefaultFuseDuration, 2);

    DangerZoneRenderer renderer(TileSize);
    renderer.update({bomb}, grid);

    EXPECT_EQ(tintedCells(renderer.getVertices()),
              (std::vector<Position>{{3, 2}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {3, 4}, {3, 5}}));
}

TEST(DangerZoneRendererTest, ABiggerBlastRadiusTintsMoreCells)
{
    const Grid grid(11, 11);
    DangerZoneRenderer renderer(TileSize);

    renderer.update({Bomb({5, 5}, Bomb::DefaultFuseDuration, 1)}, grid);
    EXPECT_EQ(tintedCells(renderer.getVertices()).size(), 5u);

    renderer.update({Bomb({5, 5}, Bomb::DefaultFuseDuration, 4)}, grid);
    EXPECT_EQ(tintedCells(renderer.getVertices()).size(), 17u);
}

TEST(DangerZoneRendererTest, TheTintIsAnInsetRoundedSquare)
{
    DangerZoneRenderer renderer(TileSize);
    renderer.update({Bomb({0, 0}, Bomb::DefaultFuseDuration, 0)}, Grid(3, 3));

    const sf::VertexArray& vertices = renderer.getVertices();
    ASSERT_GT(vertices.getVertexCount(), 0u);
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
    {
        EXPECT_GE(vertices[i].position.x, 5.f - 1e-3f);
        EXPECT_LE(vertices[i].position.x, 95.f + 1e-3f);
        EXPECT_GE(vertices[i].position.y, 5.f - 1e-3f);
        EXPECT_LE(vertices[i].position.y, 95.f + 1e-3f);
        EXPECT_EQ(vertices[i].color.r, 214);
        EXPECT_EQ(vertices[i].color.g, 60);
        EXPECT_EQ(vertices[i].color.b, 40);
    }
}

TEST(DangerZoneRendererTest, TheTintGrowsAsTheFuseBurns)
{
    Bomb bomb({1, 1});
    const float atStart = DangerZoneRenderer::tintOpacity(bomb); // 0.10 + pulse
    bomb.update(Bomb::DefaultFuseDuration * 0.95f);
    const float nearTheEnd = DangerZoneRenderer::tintOpacity(bomb);

    EXPECT_GE(atStart, 0.10f);
    EXPECT_LE(atStart, 0.18f + 1e-4f);
    EXPECT_GE(nearTheEnd, 0.10f + 0.22f * 0.95f - 1e-4f);
    EXPECT_GT(nearTheEnd, atStart);
}

TEST(DangerZoneRendererTest, ExplodedBombsLeaveNoTint)
{
    Bomb bomb({1, 1});
    bomb.update(Bomb::DefaultFuseDuration);

    DangerZoneRenderer renderer(TileSize);
    renderer.update({bomb}, Grid(3, 3));

    EXPECT_EQ(renderer.getVertices().getVertexCount(), 0u);
}

#include <gtest/gtest.h>

#include "grid_renderer.h"

using Bomberman::Grid;
using Bomberman::GridRenderer;
using Bomberman::Tile;
using Bomberman::TileType;

// These tests only inspect the generated geometry: no window nor OpenGL
// context is needed, so they run on headless machines (CI).

TEST(GridRendererTest, PixelSizeMatchesGrid)
{
    GridRenderer renderer(32.f);
    renderer.update(Grid(Grid::ClassicWidth, Grid::ClassicHeight));

    EXPECT_EQ(renderer.getPixelSize(), sf::Vector2u(15u * 32u, 13u * 32u));
}

TEST(GridRendererTest, GeneratesTwoTrianglesPerTile)
{
    GridRenderer renderer;
    renderer.update(Grid(4, 3));

    EXPECT_EQ(renderer.getVertices().getPrimitiveType(), sf::PrimitiveType::Triangles);
    EXPECT_EQ(renderer.getVertices().getVertexCount(), 4u * 3u * GridRenderer::VerticesPerTile);
}

TEST(GridRendererTest, TilesArePlacedRowByRow)
{
    GridRenderer renderer(10.f);
    renderer.update(Grid(3, 2));

    // First vertex of tile (x=2, y=1) is its top-left corner.
    const std::size_t tileIndex = 1 * 3 + 2;
    EXPECT_EQ(renderer.getVertices()[tileIndex * GridRenderer::VerticesPerTile].position, sf::Vector2f(20.f, 10.f));
}

TEST(GridRendererTest, TileColorDependsOnType)
{
    Grid grid(2, 1);
    grid.setTile({1, 0}, Tile(TileType::Wall));

    GridRenderer renderer;
    renderer.update(grid);

    EXPECT_EQ(renderer.getVertices()[0].color, GridRenderer::colorOf(TileType::Empty));
    EXPECT_EQ(renderer.getVertices()[GridRenderer::VerticesPerTile].color, GridRenderer::colorOf(TileType::Wall));
}

TEST(GridRendererTest, UpdateReflectsDestroyedBlock)
{
    Grid grid(1, 1, TileType::DestructibleBlock);
    GridRenderer renderer;
    renderer.update(grid);
    ASSERT_EQ(renderer.getVertices()[0].color, GridRenderer::colorOf(TileType::DestructibleBlock));

    grid.getTile({0, 0}).destroy();
    renderer.update(grid);

    EXPECT_EQ(renderer.getVertices()[0].color, GridRenderer::colorOf(TileType::Empty));
}

TEST(GridRendererTest, TileTypesHaveDistinctColors)
{
    EXPECT_NE(GridRenderer::colorOf(TileType::Empty), GridRenderer::colorOf(TileType::Wall));
    EXPECT_NE(GridRenderer::colorOf(TileType::Empty), GridRenderer::colorOf(TileType::DestructibleBlock));
    EXPECT_NE(GridRenderer::colorOf(TileType::Wall), GridRenderer::colorOf(TileType::DestructibleBlock));
}

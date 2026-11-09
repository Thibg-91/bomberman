#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "grid_loader.h"
#include "grid_renderer.h"
#include "map_theme.h"

using Bomberman::Grid;
using Bomberman::GridLoader;
using Bomberman::GridRenderer;
using Bomberman::MapTheme;
using Bomberman::Tile;
using Bomberman::TileType;

// These tests only inspect the generated geometry: no window nor OpenGL
// context is needed, so they run on headless machines (CI).

namespace
{

const std::filesystem::path GridDir = std::filesystem::path(BOMBERMAN_DATA_DIR) / "grid";

// 5x5 board: walls around, one pillar, one crate.
Grid smallBoard()
{
    Grid grid(5, 5);
    for (int i = 0; i < 5; ++i)
    {
        grid.setTile({i, 0}, Tile(TileType::Wall));
        grid.setTile({i, 4}, Tile(TileType::Wall));
        grid.setTile({0, i}, Tile(TileType::Wall));
        grid.setTile({4, i}, Tile(TileType::Wall));
    }
    grid.setTile({2, 2}, Tile(TileType::Wall));
    grid.setTile({1, 1}, Tile(TileType::DestructibleBlock));
    return grid;
}

} // namespace

TEST(GridRendererTest, PixelSizeMatchesGrid)
{
    GridRenderer renderer(32.f);
    renderer.update(Grid(Grid::ClassicWidth, Grid::ClassicHeight));

    EXPECT_EQ(renderer.getPixelSize(), sf::Vector2u(15u * 32u, 13u * 32u));
}

TEST(GridRendererTest, KeepsTileSizeAndTheme)
{
    const GridRenderer renderer(20.f, MapTheme::Docks);

    EXPECT_FLOAT_EQ(renderer.getTileSize(), 20.f);
    EXPECT_EQ(renderer.getTheme(), MapTheme::Docks);
}

TEST(GridRendererTest, BuildsTrianglesOnly)
{
    GridRenderer renderer;
    renderer.update(smallBoard());

    EXPECT_EQ(renderer.getVertices().getPrimitiveType(), sf::PrimitiveType::Triangles);
    EXPECT_GT(renderer.getVertices().getVertexCount(), 0u);
    EXPECT_EQ(renderer.getVertices().getVertexCount() % 3u, 0u);
}

TEST(GridRendererTest, IsDeterministic)
{
    GridRenderer first(16.f, MapTheme::Island);
    GridRenderer second(16.f, MapTheme::Island);
    first.update(smallBoard());
    second.update(smallBoard());

    ASSERT_EQ(first.getVertices().getVertexCount(), second.getVertices().getVertexCount());
    for (std::size_t i = 0; i < first.getVertices().getVertexCount(); ++i)
    {
        EXPECT_EQ(first.getVertices()[i].position, second.getVertices()[i].position);
        EXPECT_EQ(first.getVertices()[i].color, second.getVertices()[i].color);
    }
}

TEST(GridRendererTest, UpdateRemovesDestroyedCrate)
{
    Grid grid = smallBoard();
    GridRenderer renderer;
    renderer.update(grid);
    const std::size_t withCrate = renderer.getVertices().getVertexCount();

    ASSERT_TRUE(grid.getTile({1, 1}).destroy());
    renderer.update(grid);

    // The crate disappears; its cell gains the light grid lines instead.
    EXPECT_NE(renderer.getVertices().getVertexCount(), withCrate);
}

TEST(GridRendererTest, ThemesLookDifferent)
{
    GridRenderer deck(16.f, MapTheme::Deck);
    GridRenderer island(16.f, MapTheme::Island);
    deck.update(smallBoard());
    island.update(smallBoard());

    // The first vertex belongs to the top-left border tile: bulwark vs sea.
    EXPECT_NE(deck.getVertices()[0].color, island.getVertices()[0].color);
}

// Every shipped level, drawn with its own theme, stays inside the board so
// nothing spills onto the black margin around it in fullscreen.
TEST(GridRendererTest, ShippedLevelsStayInsideTheBoard)
{
    for (const char* file : {"level_01.json", "level_02.json", "level_03.json", "level_04.json"})
    {
        const Grid grid = GridLoader::loadFromFile(GridDir / file);
        GridRenderer renderer(40.f, Bomberman::mapThemeFromName(grid.getTheme()));
        renderer.update(grid);

        const sf::Vector2f size(static_cast<float>(renderer.getPixelSize().x),
                                static_cast<float>(renderer.getPixelSize().y));
        constexpr float Tolerance = 1.f;
        for (std::size_t i = 0; i < renderer.getVertices().getVertexCount(); ++i)
        {
            const sf::Vector2f p = renderer.getVertices()[i].position;
            ASSERT_GE(p.x, -Tolerance) << file;
            ASSERT_GE(p.y, -Tolerance) << file;
            ASSERT_LE(p.x, size.x + Tolerance) << file;
            ASSERT_LE(p.y, size.y + Tolerance) << file;
        }
    }
}

TEST(MapThemeTest, ParsesKnownNames)
{
    EXPECT_EQ(Bomberman::mapThemeFromName("deck"), MapTheme::Deck);
    EXPECT_EQ(Bomberman::mapThemeFromName("island"), MapTheme::Island);
    EXPECT_EQ(Bomberman::mapThemeFromName("cave"), MapTheme::Cave);
    EXPECT_EQ(Bomberman::mapThemeFromName("docks"), MapTheme::Docks);
}

TEST(MapThemeTest, UnknownOrMissingNameFallsBackToDeck)
{
    EXPECT_EQ(Bomberman::mapThemeFromName(""), MapTheme::Deck);
    EXPECT_EQ(Bomberman::mapThemeFromName("volcano"), MapTheme::Deck);
}

TEST(MapThemeTest, ShippedLevelsUseDistinctKnownThemes)
{
    const std::pair<const char*, MapTheme> expected[] = {{"level_01.json", MapTheme::Deck},
                                                         {"level_02.json", MapTheme::Island},
                                                         {"level_03.json", MapTheme::Cave},
                                                         {"level_04.json", MapTheme::Docks}};
    for (const auto& [file, theme] : expected)
        EXPECT_EQ(Bomberman::mapThemeFromName(GridLoader::loadFromFile(GridDir / file).getTheme()), theme) << file;
}

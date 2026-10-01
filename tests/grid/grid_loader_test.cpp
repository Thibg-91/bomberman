#include <algorithm>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "grid_loader.h"

using Bomberman::Grid;
using Bomberman::GridLoader;
using Bomberman::GridLoadError;
using Bomberman::Position;
using Bomberman::TileType;

namespace
{

const std::filesystem::path DataDir = BOMBERMAN_DATA_DIR;
const std::filesystem::path FirstLevel = DataDir / "grid" / "level_01.json";

} // namespace

// ---------------------------------------------------------------------------
// Parsing of a small hand-written grid
// ---------------------------------------------------------------------------
TEST(GridLoaderTest, ParsesEverySymbol)
{
    const Grid grid = GridLoader::loadFromString(R"({
        "width": 4,
        "height": 3,
        "rows": ["####",
                 "#P+#",
                 "#..#"]
    })");

    ASSERT_EQ(grid.getWidth(), 4);
    ASSERT_EQ(grid.getHeight(), 3);
    EXPECT_EQ(grid.getTile({0, 0}).getType(), TileType::Wall);
    EXPECT_EQ(grid.getTile({1, 1}).getType(), TileType::Empty);
    EXPECT_EQ(grid.getTile({2, 1}).getType(), TileType::DestructibleBlock);
    EXPECT_EQ(grid.getTile({2, 2}).getType(), TileType::Empty);
    ASSERT_EQ(grid.getSpawnPoints().size(), 1u);
    EXPECT_EQ(grid.getSpawnPoints().front(), (Position{1, 1}));
}

// ---------------------------------------------------------------------------
// Invalid descriptions are rejected with a GridLoadError
// ---------------------------------------------------------------------------
TEST(GridLoaderTest, RejectsMalformedJson)
{
    EXPECT_THROW((void)GridLoader::loadFromString("{ \"width\": "), GridLoadError);
    EXPECT_THROW((void)GridLoader::loadFromString("[1, 2, 3]"), GridLoadError);
}

TEST(GridLoaderTest, RejectsMissingFields)
{
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"height": 1, "rows": ["."]})"), GridLoadError);
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"width": 1, "rows": ["."]})"), GridLoadError);
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"width": 1, "height": 1})"), GridLoadError);
}

TEST(GridLoaderTest, RejectsNonPositiveDimensions)
{
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"width": 0, "height": 1, "rows": [""]})"), GridLoadError);
}

TEST(GridLoaderTest, RejectsWrongRowCount)
{
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"width": 2, "height": 3, "rows": ["..", ".."]})"),
                 GridLoadError);
}

TEST(GridLoaderTest, RejectsWrongRowLength)
{
    EXPECT_THROW((void)GridLoader::loadFromString(R"({"width": 3, "height": 2, "rows": ["...", "...."]})"),
                 GridLoadError);
}

TEST(GridLoaderTest, RejectsUnknownSymbol)
{
    try
    {
        (void)GridLoader::loadFromString(R"({"width": 3, "height": 1, "rows": [".?."]})");
        FAIL() << "Expected GridLoadError";
    }
    catch (const GridLoadError& error)
    {
        EXPECT_NE(std::string(error.what()).find("'?'"), std::string::npos) << error.what();
    }
}

TEST(GridLoaderTest, RejectsMissingFile)
{
    EXPECT_THROW((void)GridLoader::loadFromFile(DataDir / "grid" / "does_not_exist.json"), GridLoadError);
}

// ---------------------------------------------------------------------------
// The first level shipped with the game is a conventional Bomberman board
// ---------------------------------------------------------------------------
class FirstLevelTest : public ::testing::Test
{
protected:
    const Grid m_grid = GridLoader::loadFromFile(FirstLevel);
};

TEST_F(FirstLevelTest, HasClassicDimensions)
{
    EXPECT_EQ(m_grid.getWidth(), Grid::ClassicWidth);
    EXPECT_EQ(m_grid.getHeight(), Grid::ClassicHeight);
}

TEST_F(FirstLevelTest, IsSurroundedByWalls)
{
    for (int x = 0; x < m_grid.getWidth(); ++x)
    {
        EXPECT_EQ(m_grid.getTile({x, 0}).getType(), TileType::Wall) << "x=" << x;
        EXPECT_EQ(m_grid.getTile({x, m_grid.getHeight() - 1}).getType(), TileType::Wall) << "x=" << x;
    }
    for (int y = 0; y < m_grid.getHeight(); ++y)
    {
        EXPECT_EQ(m_grid.getTile({0, y}).getType(), TileType::Wall) << "y=" << y;
        EXPECT_EQ(m_grid.getTile({m_grid.getWidth() - 1, y}).getType(), TileType::Wall) << "y=" << y;
    }
}

TEST_F(FirstLevelTest, HasPillarsOnEvenCells)
{
    for (int y = 2; y < m_grid.getHeight() - 1; y += 2)
        for (int x = 2; x < m_grid.getWidth() - 1; x += 2)
            EXPECT_EQ(m_grid.getTile({x, y}).getType(), TileType::Wall) << "(" << x << ", " << y << ")";
}

TEST_F(FirstLevelTest, HasOneSpawnPerCorner)
{
    const auto& spawns = m_grid.getSpawnPoints();
    const int right = m_grid.getWidth() - 2;
    const int bottom = m_grid.getHeight() - 2;

    ASSERT_EQ(spawns.size(), 4u);
    for (const Position corner : {Position{1, 1}, Position{right, 1}, Position{1, bottom}, Position{right, bottom}})
        EXPECT_NE(std::find(spawns.begin(), spawns.end(), corner), spawns.end())
            << "No spawn at (" << corner.x << ", " << corner.y << ")";
}

TEST_F(FirstLevelTest, SpawnsCanMoveAway)
{
    // A player must be able to step aside to dodge its first bomb.
    for (const Position spawn : m_grid.getSpawnPoints())
    {
        int freeNeighbours = 0;
        for (const Position offset : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}})
            freeNeighbours += m_grid.isWalkable({spawn.x + offset.x, spawn.y + offset.y}) ? 1 : 0;

        EXPECT_GE(freeNeighbours, 2) << "Spawn (" << spawn.x << ", " << spawn.y << ") is trapped";
    }
}

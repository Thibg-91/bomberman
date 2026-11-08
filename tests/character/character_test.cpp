#include <filesystem>

#include <gtest/gtest.h>

#include "character.h"
#include "grid_loader.h"

using Bomberman::Character;
using Bomberman::CharacterCommand;
using Bomberman::Direction;
using Bomberman::Grid;
using Bomberman::Position;
using Bomberman::Tile;
using Bomberman::TileType;

namespace
{

// Walkable 5x5 board; tests add obstacles.
Grid openGrid()
{
    return Grid(5, 5);
}

} // namespace

TEST(DirectionTest, StepMovesOneCell)
{
    EXPECT_EQ(Bomberman::step({2, 2}, Direction::Up), (Position{2, 1}));
    EXPECT_EQ(Bomberman::step({2, 2}, Direction::Down), (Position{2, 3}));
    EXPECT_EQ(Bomberman::step({2, 2}, Direction::Left), (Position{1, 2}));
    EXPECT_EQ(Bomberman::step({2, 2}, Direction::Right), (Position{3, 2}));
}

TEST(CharacterCommandTest, Equality)
{
    EXPECT_EQ(CharacterCommand::move(Direction::Up), CharacterCommand::move(Direction::Up));
    EXPECT_NE(CharacterCommand::move(Direction::Up), CharacterCommand::move(Direction::Left));
    EXPECT_NE(CharacterCommand::move(Direction::Up), CharacterCommand::breakBlock());
    EXPECT_EQ(CharacterCommand::breakBlock(), CharacterCommand::breakBlock());
}

TEST(CharacterTest, StartsIdleOnItsSpawn)
{
    const Character character({1, 3});

    EXPECT_EQ(character.getPosition(), (Position{1, 3}));
    EXPECT_EQ(character.getPreviousPosition(), (Position{1, 3}));
    EXPECT_FALSE(character.isMoving());
    EXPECT_FLOAT_EQ(character.getMoveProgress(), 1.f);
}

TEST(CharacterTest, MovesOneCellInAllDirections)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 0.f);

    EXPECT_TRUE(character.move(Direction::Up, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 1}));
    EXPECT_TRUE(character.move(Direction::Right, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 1}));
    EXPECT_TRUE(character.move(Direction::Down, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 2}));
    EXPECT_TRUE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
}

TEST(CharacterTest, WallsAndBlocksStopTheCharacter)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::Wall));
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);

    EXPECT_FALSE(character.move(Direction::Right, grid));
    EXPECT_FALSE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
}

TEST(CharacterTest, CannotLeaveTheGrid)
{
    const Grid grid = openGrid();
    Character character({0, 0}, 0.f);

    EXPECT_FALSE(character.move(Direction::Up, grid));
    EXPECT_FALSE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{0, 0}));
}

TEST(CharacterTest, BlockedMoveStillTurnsTheCharacter)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::Wall));
    Character character({2, 2}, 0.f);

    character.move(Direction::Right, grid);

    EXPECT_EQ(character.getFacing(), Direction::Right);
}

TEST(CharacterTest, CannotMoveWhileMoving)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 1.f);

    ASSERT_TRUE(character.move(Direction::Right, grid));
    EXPECT_TRUE(character.isMoving());
    EXPECT_EQ(character.getPreviousPosition(), (Position{2, 2}));

    EXPECT_FALSE(character.move(Direction::Down, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 2}));
    EXPECT_EQ(character.getFacing(), Direction::Right);
}

TEST(CharacterTest, UpdateProgressesAndFinishesTheMove)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 1.f);
    ASSERT_TRUE(character.move(Direction::Right, grid));

    EXPECT_FLOAT_EQ(character.getMoveProgress(), 0.f);
    character.update(0.25f);
    EXPECT_FLOAT_EQ(character.getMoveProgress(), 0.25f);
    EXPECT_TRUE(character.isMoving());

    character.update(10.f);
    EXPECT_FALSE(character.isMoving());
    EXPECT_FLOAT_EQ(character.getMoveProgress(), 1.f);
    EXPECT_TRUE(character.move(Direction::Right, grid));
}

TEST(CharacterTest, NegativeDeltaIsIgnored)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 1.f);
    ASSERT_TRUE(character.move(Direction::Right, grid));

    character.update(-5.f);

    EXPECT_FLOAT_EQ(character.getMoveProgress(), 0.f);
}

TEST(CharacterTest, BreaksTheDestructibleBlockInFront)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);
    character.move(Direction::Right, grid); // blocked: only turns

    EXPECT_TRUE(character.breakBlock(grid));

    EXPECT_EQ(grid.getTile({3, 2}).getType(), TileType::Empty);
    EXPECT_TRUE(character.move(Direction::Right, grid));
}

TEST(CharacterTest, OnlyBreaksTheFacedCell)
{
    Grid grid = openGrid();
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    grid.setTile({2, 1}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);
    character.move(Direction::Up, grid);

    EXPECT_TRUE(character.breakBlock(grid));

    EXPECT_EQ(grid.getTile({2, 1}).getType(), TileType::Empty);
    EXPECT_EQ(grid.getTile({1, 2}).getType(), TileType::DestructibleBlock);
}

TEST(CharacterTest, CannotBreakWallsEmptyCellsOrOutsideTheGrid)
{
    Grid grid = openGrid();
    grid.setTile({3, 0}, Tile(TileType::Wall));
    Character character({3, 1}, 0.f);

    character.move(Direction::Up, grid);
    EXPECT_FALSE(character.breakBlock(grid));
    EXPECT_EQ(grid.getTile({3, 0}).getType(), TileType::Wall);

    character.move(Direction::Down, grid);
    EXPECT_FALSE(character.breakBlock(grid)); // empty cell ahead

    Character edge({0, 0}, 0.f);
    edge.move(Direction::Up, grid);
    EXPECT_FALSE(edge.breakBlock(grid)); // outside
}

TEST(CharacterTest, CannotBreakWhileMoving)
{
    Grid grid = openGrid();
    grid.setTile({4, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 1.f);
    ASSERT_TRUE(character.move(Direction::Right, grid));

    EXPECT_FALSE(character.breakBlock(grid));
    EXPECT_EQ(grid.getTile({4, 2}).getType(), TileType::DestructibleBlock);

    character.update(1.f);
    EXPECT_TRUE(character.breakBlock(grid));
}

TEST(CharacterTest, ExecuteDispatchesCommands)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);

    EXPECT_TRUE(character.execute(CharacterCommand::move(Direction::Up), grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 1}));
    EXPECT_TRUE(character.execute(CharacterCommand::move(Direction::Down), grid));
    EXPECT_FALSE(character.execute(CharacterCommand::move(Direction::Right), grid));
    EXPECT_TRUE(character.execute(CharacterCommand::breakBlock(), grid));
    EXPECT_FALSE(character.execute(CharacterCommand::breakBlock(), grid));
}

TEST(CharacterTest, WorksOnTheFirstLevel)
{
    Grid grid = Bomberman::GridLoader::loadFromFile(std::filesystem::path(BOMBERMAN_DATA_DIR) / "grid" / "level_01.json");
    Character character(grid.getSpawnPoints().front(), 0.f);

    // The spawn is in a corner: walls above and on the left, free cell on the right.
    EXPECT_FALSE(character.move(Direction::Up, grid));
    EXPECT_FALSE(character.move(Direction::Left, grid));
    EXPECT_TRUE(character.move(Direction::Right, grid));
}

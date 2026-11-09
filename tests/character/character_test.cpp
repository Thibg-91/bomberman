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

// Characters start facing down; a move towards another direction only turns.
void face(Character& character, Direction direction, const Grid& grid)
{
    character.move(direction, grid);
    ASSERT_EQ(character.getFacing(), direction);
}

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
    EXPECT_NE(CharacterCommand::move(Direction::Up), CharacterCommand::pushBomb());
    EXPECT_EQ(CharacterCommand::pushBomb(), CharacterCommand::pushBomb());
    EXPECT_EQ(CharacterCommand::placeBomb(), CharacterCommand::placeBomb());
    EXPECT_NE(CharacterCommand::placeBomb(), CharacterCommand::pushBomb());
}

TEST(CharacterTest, StartsIdleOnItsSpawn)
{
    const Character character({1, 3});

    EXPECT_EQ(character.getPosition(), (Position{1, 3}));
    EXPECT_EQ(character.getPreviousPosition(), (Position{1, 3}));
    EXPECT_FALSE(character.isMoving());
    EXPECT_FLOAT_EQ(character.getMoveProgress(), 1.f);
    EXPECT_TRUE(character.isAlive());
}

TEST(CharacterTest, KillMarksTheCharacterAsDead)
{
    Character character({1, 3});

    character.kill();

    EXPECT_FALSE(character.isAlive());
}

TEST(CharacterTest, MovesOneCellInAllDirections)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 0.f);

    face(character, Direction::Up, grid);
    EXPECT_TRUE(character.move(Direction::Up, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 1}));
    face(character, Direction::Right, grid);
    EXPECT_TRUE(character.move(Direction::Right, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 1}));
    face(character, Direction::Down, grid);
    EXPECT_TRUE(character.move(Direction::Down, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 2}));
    face(character, Direction::Left, grid);
    EXPECT_TRUE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
}

TEST(CharacterTest, FirstMoveTowardsANewDirectionOnlyTurns)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 1.f);
    ASSERT_EQ(character.getFacing(), Direction::Down);

    EXPECT_FALSE(character.move(Direction::Right, grid));
    EXPECT_EQ(character.getFacing(), Direction::Right);
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
    EXPECT_FALSE(character.isMoving()); // turning takes no time

    EXPECT_TRUE(character.move(Direction::Right, grid));
    EXPECT_EQ(character.getPosition(), (Position{3, 2}));
}

TEST(CharacterTest, MovesStraightAwayWhenAlreadyFacingTheDirection)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 0.f);

    EXPECT_TRUE(character.move(Direction::Down, grid)); // initial facing
    EXPECT_TRUE(character.move(Direction::Down, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 4}));
}

TEST(CharacterTest, WallsAndBlocksStopTheCharacter)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::Wall));
    grid.setTile({1, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);

    face(character, Direction::Right, grid);
    EXPECT_FALSE(character.move(Direction::Right, grid));
    face(character, Direction::Left, grid);
    EXPECT_FALSE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
}

TEST(CharacterTest, ObstaclesStopTheCharacter)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 0.f);
    const std::vector<Position> bombs = {{3, 2}};
    face(character, Direction::Right, grid);

    EXPECT_FALSE(character.move(Direction::Right, grid, bombs));
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
    EXPECT_FALSE(character.execute(CharacterCommand::move(Direction::Right), grid, bombs));
    face(character, Direction::Down, grid);
    EXPECT_TRUE(character.execute(CharacterCommand::move(Direction::Down), grid, bombs));
}

TEST(CharacterTest, CanLeaveACellHoldingAnObstacle)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 0.f);

    // e.g. a bomb pushed under the character: only the target cell matters.
    EXPECT_TRUE(character.move(Direction::Down, grid, {{2, 2}}));
}

TEST(CharacterTest, CannotLeaveTheGrid)
{
    const Grid grid = openGrid();
    Character character({0, 0}, 0.f);

    face(character, Direction::Up, grid);
    EXPECT_FALSE(character.move(Direction::Up, grid));
    face(character, Direction::Left, grid);
    EXPECT_FALSE(character.move(Direction::Left, grid));
    EXPECT_EQ(character.getPosition(), (Position{0, 0}));
}

TEST(CharacterTest, CanTurnTowardsABlockedCell)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::Wall));
    Character character({2, 2}, 0.f);

    EXPECT_FALSE(character.move(Direction::Right, grid));

    EXPECT_EQ(character.getFacing(), Direction::Right);
}

TEST(CharacterTest, CannotMoveWhileMoving)
{
    const Grid grid = openGrid();
    Character character({2, 2}, 1.f);
    face(character, Direction::Right, grid);

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
    face(character, Direction::Right, grid);
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
    face(character, Direction::Right, grid);
    ASSERT_TRUE(character.move(Direction::Right, grid));

    character.update(-5.f);

    EXPECT_FLOAT_EQ(character.getMoveProgress(), 0.f);
}

TEST(CharacterTest, ExecuteDispatchesCommands)
{
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);

    EXPECT_FALSE(character.execute(CharacterCommand::move(Direction::Up), grid)); // turns
    EXPECT_TRUE(character.execute(CharacterCommand::move(Direction::Up), grid));
    EXPECT_EQ(character.getPosition(), (Position{2, 1}));
    face(character, Direction::Down, grid);
    EXPECT_TRUE(character.execute(CharacterCommand::move(Direction::Down), grid));
    face(character, Direction::Right, grid);
    EXPECT_FALSE(character.execute(CharacterCommand::move(Direction::Right), grid));
    // Bomb commands are handled by the caller, which owns the BombManager.
    EXPECT_FALSE(character.execute(CharacterCommand::pushBomb(), grid));
    EXPECT_FALSE(character.execute(CharacterCommand::placeBomb(), grid));
}

TEST(CharacterTest, NoCommandBreaksTheBlockInFront)
{
    // Only bomb explosions destroy blocks.
    Grid grid = openGrid();
    grid.setTile({3, 2}, Tile(TileType::DestructibleBlock));
    Character character({2, 2}, 0.f);
    character.move(Direction::Right, grid); // blocked: only turns towards the block

    for (const CharacterCommand command : {CharacterCommand::pushBomb(), CharacterCommand::placeBomb(),
                                           CharacterCommand::move(Direction::Right)})
        character.execute(command, grid);

    EXPECT_EQ(grid.getTile({3, 2}).getType(), TileType::DestructibleBlock);
    EXPECT_EQ(character.getPosition(), (Position{2, 2}));
}

TEST(CharacterTest, WorksOnTheFirstLevel)
{
    Grid grid = Bomberman::GridLoader::loadFromFile(std::filesystem::path(BOMBERMAN_DATA_DIR) / "grid" / "level_01.json");
    Character character(grid.getSpawnPoints().front(), 0.f);

    // The spawn is in a corner: walls above and on the left, free cell on the right.
    face(character, Direction::Up, grid);
    EXPECT_FALSE(character.move(Direction::Up, grid));
    face(character, Direction::Left, grid);
    EXPECT_FALSE(character.move(Direction::Left, grid));
    face(character, Direction::Right, grid);
    EXPECT_TRUE(character.move(Direction::Right, grid));
}

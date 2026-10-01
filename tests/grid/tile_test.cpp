#include <gtest/gtest.h>

#include "tile.h"

using Bomberman::Tile;
using Bomberman::TileType;

TEST(TileTest, DefaultTileIsEmpty)
{
    EXPECT_EQ(Tile().getType(), TileType::Empty);
}

TEST(TileTest, OnlyEmptyTileIsWalkable)
{
    EXPECT_TRUE(Tile(TileType::Empty).isWalkable());
    EXPECT_FALSE(Tile(TileType::Wall).isWalkable());
    EXPECT_FALSE(Tile(TileType::DestructibleBlock).isWalkable());
}

TEST(TileTest, WallAndBlockStopExplosions)
{
    EXPECT_FALSE(Tile(TileType::Empty).blocksExplosion());
    EXPECT_TRUE(Tile(TileType::Wall).blocksExplosion());
    EXPECT_TRUE(Tile(TileType::DestructibleBlock).blocksExplosion());
}

TEST(TileTest, DestroyingBlockLeavesEmptyTile)
{
    Tile block(TileType::DestructibleBlock);

    EXPECT_TRUE(block.destroy());
    EXPECT_EQ(block.getType(), TileType::Empty);
    EXPECT_TRUE(block.isWalkable());
}

TEST(TileTest, WallCannotBeDestroyed)
{
    Tile wall(TileType::Wall);

    EXPECT_FALSE(wall.destroy());
    EXPECT_EQ(wall.getType(), TileType::Wall);
}

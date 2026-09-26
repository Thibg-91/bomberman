#ifndef TILE_H
#define TILE_H

#pragma once

// TODO: Watch what is needed to keep
#include <string>
#include <memory>
#include <vector>

namespace Bomberman
{
enum class TileType
{
    Empty,
    Wall,
    Bonus
};

class Tile
{

public:
   Tile(TileType);

private:
    TileType _type;

};

} 

#endif // TILE_H
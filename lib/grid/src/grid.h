#ifndef GRID_H
#define GRID_H

#pragma once

// TODO: Watch what is needed to keep
#include <string>
#include <memory>
#include <vector>
#include <filesystem>

#include "tile.h"

namespace Bomberman
{
class Grid
{
public:
    explicit Grid();

   void loadGrid(std::filesystem::path grid_file);

private:
   std::vector<Bomberman::Tile> _columns;
   std::vector<Bomberman::Tile> _lines;
};

} 

#endif // GRID_H
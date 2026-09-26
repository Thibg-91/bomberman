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
    Grid();
    explicit Grid(const std::filesystem::path& grid_file);

   void loadGrid() const;

private:
   std::vector<Bomberman::Tile> columns;
   std::vector<Bomberman::Tile> lines;
};

} 

#endif // GRID_H
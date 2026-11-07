#ifndef GRID_LOADER_H
#define GRID_LOADER_H

#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "grid.h"

namespace Bomberman
{

class GridLoadError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// Builds a Grid from a JSON level description:
//
// {
//   "width": 15,
//   "height": 13,
//   "rows": [ "###############", "#P.+++...", ... ]
// }
//
// Legend: '#' wall, '+' destructible block, '.' empty, 'P' player spawn
// (an empty tile registered as spawn point).
class GridLoader
{
public:
    static constexpr char WallSymbol = '#';
    static constexpr char DestructibleBlockSymbol = '+';
    static constexpr char EmptySymbol = '.';
    static constexpr char SpawnSymbol = 'P';

    // Throw GridLoadError if the content cannot be read or is invalid.
    [[nodiscard]] static Grid loadFromFile(const std::filesystem::path& gridFile);
    [[nodiscard]] static Grid loadFromString(const std::string& jsonContent);
};

} // namespace Bomberman

#endif // GRID_LOADER_H

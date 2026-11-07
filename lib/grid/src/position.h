#ifndef POSITION_H
#define POSITION_H

#pragma once

namespace Bomberman
{

// Cell coordinates on the grid: x is the column, y is the row, (0, 0) is the
// top-left corner. Shared by the grid and, later, by players, bombs and items.
struct Position
{
    int x = 0;
    int y = 0;

    friend bool operator==(const Position& lhs, const Position& rhs) noexcept
    {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }

    friend bool operator!=(const Position& lhs, const Position& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace Bomberman

#endif // POSITION_H

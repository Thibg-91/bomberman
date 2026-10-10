#ifndef DIRECTION_H
#define DIRECTION_H

#pragma once

#include "position.h"

namespace Bomberman
{

enum class Direction
{
    Up,
    Down,
    Left,
    Right
};

// Cell reached by moving one step from position in the given direction.
// y grows downwards, so Up decreases y.
[[nodiscard]] constexpr Position step(Position position, Direction direction) noexcept
{
    switch (direction)
    {
    case Direction::Up:
        return {position.x, position.y - 1};
    case Direction::Down:
        return {position.x, position.y + 1};
    case Direction::Left:
        return {position.x - 1, position.y};
    case Direction::Right:
        return {position.x + 1, position.y};
    }
    return position;
}

} // namespace Bomberman

#endif // DIRECTION_H

#ifndef BLAST_H
#define BLAST_H

#pragma once

#include <array>
#include <vector>

#include "direction.h"
#include "grid.h"
#include "position.h"

namespace Bomberman
{

// The cross of cells reached by a bomb's explosion: its center plus, for
// each direction, how many cells the arm covers. An arm stops before a cell
// that blocks explosions (wall, grid edge) and includes the destructible
// block it hits, if any. Used both to apply an explosion and to show where
// a ticking bomb is going to hit.
struct Blast
{
    static constexpr std::array<Direction, 4> Directions = {Direction::Up, Direction::Down, Direction::Left,
                                                            Direction::Right};

    Position center;
    std::array<int, 4> reach{}; // cells per arm, indexed like Directions

    // The blast of a bomb of blastRadius on center, on the grid as it is
    // now (nothing is destroyed).
    [[nodiscard]] static Blast compute(Position center, int blastRadius, const Grid& grid);

    [[nodiscard]] int reachTowards(Direction direction) const noexcept;

    // Center first, then each arm from the center outwards, in Directions
    // order.
    [[nodiscard]] std::vector<Position> cells() const;

    friend bool operator==(const Blast& lhs, const Blast& rhs) noexcept
    {
        return lhs.center == rhs.center && lhs.reach == rhs.reach;
    }

    friend bool operator!=(const Blast& lhs, const Blast& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace Bomberman

#endif // BLAST_H

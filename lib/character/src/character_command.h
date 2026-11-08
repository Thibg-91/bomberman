#ifndef CHARACTER_COMMAND_H
#define CHARACTER_COMMAND_H

#pragma once

#include "direction.h"

namespace Bomberman
{

// What a character is asked to do. This is the only way to drive a Character:
// keyboard input, an AI or a network message all end up producing commands,
// so the character logic does not depend on where they come from.
struct CharacterCommand
{
    enum class Type
    {
        Move,
        BreakBlock
    };

    Type type = Type::Move;
    Direction direction = Direction::Down; // only used by Move

    [[nodiscard]] static constexpr CharacterCommand move(Direction direction) noexcept
    {
        return {Type::Move, direction};
    }

    [[nodiscard]] static constexpr CharacterCommand breakBlock() noexcept
    {
        return {Type::BreakBlock, Direction::Down};
    }

    friend constexpr bool operator==(const CharacterCommand& lhs, const CharacterCommand& rhs) noexcept
    {
        return lhs.type == rhs.type && (lhs.type != Type::Move || lhs.direction == rhs.direction);
    }

    friend constexpr bool operator!=(const CharacterCommand& lhs, const CharacterCommand& rhs) noexcept
    {
        return !(lhs == rhs);
    }
};

} // namespace Bomberman

#endif // CHARACTER_COMMAND_H

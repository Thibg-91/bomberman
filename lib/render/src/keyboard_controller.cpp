#include "keyboard_controller.h"

#include <array>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
KeyboardController::KeyboardController(Bindings bindings) noexcept
    : m_bindings(bindings)
{
}

// =========================
// Public Methods
// =========================
const KeyboardController::Bindings& KeyboardController::getBindings() const noexcept
{
    return m_bindings;
}

std::optional<CharacterCommand> KeyboardController::commandFor(sf::Keyboard::Key key) const noexcept
{
    if (key == m_bindings.pushBomb)
        return CharacterCommand::pushBomb();
    if (key == m_bindings.placeBomb)
        return CharacterCommand::placeBomb();
    if (key == m_bindings.up)
        return CharacterCommand::move(Direction::Up);
    if (key == m_bindings.down)
        return CharacterCommand::move(Direction::Down);
    if (key == m_bindings.left)
        return CharacterCommand::move(Direction::Left);
    if (key == m_bindings.right)
        return CharacterCommand::move(Direction::Right);
    return std::nullopt;
}

std::optional<CharacterCommand> KeyboardController::poll() const
{
    const std::array keys = {m_bindings.pushBomb, m_bindings.placeBomb, m_bindings.up, m_bindings.down,
                              m_bindings.left, m_bindings.right};
    for (const sf::Keyboard::Key key : keys)
    {
        if (sf::Keyboard::isKeyPressed(key))
            return commandFor(key);
    }
    return std::nullopt;
}

bool KeyboardController::isHeld(Direction direction) const
{
    switch (direction)
    {
    case Direction::Up:
        return sf::Keyboard::isKeyPressed(m_bindings.up);
    case Direction::Down:
        return sf::Keyboard::isKeyPressed(m_bindings.down);
    case Direction::Left:
        return sf::Keyboard::isKeyPressed(m_bindings.left);
    case Direction::Right:
        return sf::Keyboard::isKeyPressed(m_bindings.right);
    }
    return false;
}

} // namespace Bomberman

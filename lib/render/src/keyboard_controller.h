#ifndef KEYBOARD_CONTROLLER_H
#define KEYBOARD_CONTROLLER_H

#pragma once

#include <optional>

#include <SFML/Window/Keyboard.hpp>

#include "character_command.h"

namespace Bomberman
{

// Translates keyboard state into CharacterCommand. Defaults: z/q/s/d to move
// and Enter to break the block in front of the character.
class KeyboardController
{
public:
    struct Bindings
    {
        sf::Keyboard::Key up = sf::Keyboard::Key::Z;
        sf::Keyboard::Key left = sf::Keyboard::Key::Q;
        sf::Keyboard::Key down = sf::Keyboard::Key::S;
        sf::Keyboard::Key right = sf::Keyboard::Key::D;
        sf::Keyboard::Key breakBlock = sf::Keyboard::Key::Enter;
    };

    // =========================
    // Constructors / Destructor
    // =========================
    KeyboardController() = default;
    explicit KeyboardController(Bindings bindings) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] const Bindings& getBindings() const noexcept;

    // Command bound to a key, if any.
    [[nodiscard]] std::optional<CharacterCommand> commandFor(sf::Keyboard::Key key) const noexcept;

    // Command for the keys currently held (real keyboard state). Break wins
    // over movement; among movement keys: up, down, left, right.
    [[nodiscard]] std::optional<CharacterCommand> poll() const;

private:
    // =========================
    // Members
    // =========================
    Bindings m_bindings;
};

} // namespace Bomberman

#endif // KEYBOARD_CONTROLLER_H

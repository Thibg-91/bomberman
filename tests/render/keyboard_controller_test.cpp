#include <gtest/gtest.h>

#include "keyboard_controller.h"

using Bomberman::CharacterCommand;
using Bomberman::Direction;
using Bomberman::KeyboardController;
using Key = sf::Keyboard::Key;

TEST(KeyboardControllerTest, DefaultBindingsAreZqsdAndEnter)
{
    const KeyboardController keyboard;

    EXPECT_EQ(keyboard.commandFor(Key::Z), CharacterCommand::move(Direction::Up));
    EXPECT_EQ(keyboard.commandFor(Key::Q), CharacterCommand::move(Direction::Left));
    EXPECT_EQ(keyboard.commandFor(Key::S), CharacterCommand::move(Direction::Down));
    EXPECT_EQ(keyboard.commandFor(Key::D), CharacterCommand::move(Direction::Right));
    EXPECT_EQ(keyboard.commandFor(Key::Enter), CharacterCommand::breakBlock());
}

TEST(KeyboardControllerTest, UnboundKeyGivesNoCommand)
{
    EXPECT_FALSE(KeyboardController().commandFor(Key::A).has_value());
}

TEST(KeyboardControllerTest, BindingsCanBeCustomized)
{
    KeyboardController::Bindings bindings;
    bindings.up = Key::Up;
    bindings.breakBlock = Key::Space;
    const KeyboardController keyboard(bindings);

    EXPECT_EQ(keyboard.commandFor(Key::Up), CharacterCommand::move(Direction::Up));
    EXPECT_EQ(keyboard.commandFor(Key::Space), CharacterCommand::breakBlock());
    EXPECT_FALSE(keyboard.commandFor(Key::Z).has_value());
}

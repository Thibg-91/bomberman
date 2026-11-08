#ifndef CHARACTER_RENDERER_H
#define CHARACTER_RENDERER_H

#pragma once

#include <SFML/Graphics.hpp>

#include "character.h"

namespace Bomberman
{

// Draws a Character on top of a GridRenderer (use the same tile size). While
// the character walks, its sprite slides from the previous cell to the new
// one and hops up and down.
class CharacterRenderer : public sf::Drawable, public sf::Transformable
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    explicit CharacterRenderer(float tileSize, sf::Color color = sf::Color(240, 240, 240));

    // =========================
    // Public Methods
    // =========================
    // Refresh the shapes from the character state; call it every frame.
    void update(const Character& character);

    // Center of the character in pixels, relative to the grid origin.
    [[nodiscard]] sf::Vector2f getCenter() const noexcept;
    [[nodiscard]] const sf::CircleShape& getBody() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // =========================
    // Members
    // =========================
    float m_tileSize;
    sf::Vector2f m_center;
    sf::CircleShape m_body;
    sf::CircleShape m_eye;
};

} // namespace Bomberman

#endif // CHARACTER_RENDERER_H

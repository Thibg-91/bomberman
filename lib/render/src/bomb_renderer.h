#ifndef BOMB_RENDERER_H
#define BOMB_RENDERER_H

#pragma once

#include <SFML/Graphics.hpp>

#include "bomb.h"

namespace Bomberman
{

// Draws a Bomb on top of a GridRenderer (use the same tile size): a plain
// dark circle with a curved fuse topped by a spark that blinks faster and a
// body that pulses as the fuse runs out ("design A").
class BombRenderer : public sf::Drawable, public sf::Transformable
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    explicit BombRenderer(float tileSize, sf::Color color = sf::Color(28, 28, 28));

    // =========================
    // Public Methods
    // =========================
    // Refresh the shapes from the bomb state; call it every frame.
    void update(const Bomb& bomb);

    [[nodiscard]] sf::Vector2f getCenter() const noexcept;
    [[nodiscard]] const sf::CircleShape& getBody() const noexcept;
    [[nodiscard]] const sf::CircleShape& getSpark() const noexcept;
    [[nodiscard]] const sf::RectangleShape& getFuse() const noexcept;

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
    sf::RectangleShape m_fuse;
    sf::CircleShape m_spark;
};

} // namespace Bomberman

#endif // BOMB_RENDERER_H

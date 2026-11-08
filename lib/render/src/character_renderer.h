#ifndef CHARACTER_RENDERER_H
#define CHARACTER_RENDERER_H

#pragma once

#include <array>

#include <SFML/Graphics.hpp>

#include "character.h"

namespace Bomberman
{

// Draws a Character on top of a GridRenderer (use the same tile size) as a
// "bandana buccaneer": a round body, a knotted bandana with flapping tails,
// one big eye, an eyepatch and a small scar. While the character walks, it
// slides from the previous cell to the new one, hops and steps; dropping a
// bomb plays a short arms-up pose with a spark flashing where it landed.
//
// The artwork changes with Character::getFacing(): facing down (the default)
// shows the face described above; facing up shows the back of the head (no
// face, the bandana covers nearly all of it); facing left/right shows a
// side profile (a single centered arm/leg/ear, the visible eye and scar on
// the leading side, no eyepatch/strap since it is on the hidden cheek).
class CharacterRenderer : public sf::Drawable, public sf::Transformable
{
public:
    // Only the bandana (knot, tails, dots) changes between players; skin and
    // outline stay the same so every crewmate reads the same way on screen.
    struct Palette
    {
        sf::Color skin;
        sf::Color bandana;

        Palette() noexcept; // default crew colors, defined out of line
        Palette(sf::Color skinColor, sf::Color bandanaColor) noexcept : skin(skinColor), bandana(bandanaColor) {}
    };

    static constexpr int PlayerCount = 4;
    static constexpr float BombAnimDuration = 0.5f; // seconds, arms-up pose

    // =========================
    // Static Methods
    // =========================
    // One of the PlayerCount preset crew colors; out-of-range indices clamp.
    [[nodiscard]] static Palette paletteForPlayer(int playerIndex) noexcept;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit CharacterRenderer(float tileSize, Palette palette = Palette{});

    // =========================
    // Public Methods
    // =========================
    // Refresh the shapes from the character state; call it every frame.
    // deltaSeconds advances the bomb-drop pose started by triggerBombAnimation();
    // it can be left at 0 for tests that only check the idle/walk pose.
    void update(const Character& character, float deltaSeconds = 0.f);

    // Starts the short arms-up pose and ground spark; call it once a bomb has
    // actually been placed in front of the character.
    void triggerBombAnimation() noexcept;
    [[nodiscard]] bool isPlayingBombAnimation() const noexcept;

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
    float m_bombAnimElapsed; // >= BombAnimDuration means idle

    sf::CircleShape m_body;
    sf::CircleShape m_armLeft;
    sf::CircleShape m_armRight;
    sf::RectangleShape m_legLeft;
    sf::RectangleShape m_legRight;

    sf::ConvexShape m_bandanaCap;
    std::array<sf::ConvexShape, 4> m_bandanaPennants;
    sf::CircleShape m_knot;
    sf::ConvexShape m_tailLeft;
    sf::ConvexShape m_tailRight;
    std::array<sf::CircleShape, 4> m_bandanaDots; // facing up uses all 4, others only the first 3

    sf::CircleShape m_eye;
    sf::CircleShape m_eyeShine;
    sf::CircleShape m_blushLeft;
    sf::CircleShape m_blushRight;
    sf::VertexArray m_smile;
    std::array<sf::Vector2f, 5> m_smileBase; // points relative to the body center

    sf::RectangleShape m_eyepatchStrap;
    sf::CircleShape m_eyepatch;
    sf::RectangleShape m_scar;

    // Single centered ear (facing down/left/right only) and the back-of-head
    // panel (facing up only).
    sf::CircleShape m_earOuter;
    sf::CircleShape m_earInner;
    sf::CircleShape m_backPanel;

    sf::CircleShape m_spark;

    // Remembered so draw() knows which of the above to skip.
    Direction m_facing;
};

} // namespace Bomberman

#endif // CHARACTER_RENDERER_H

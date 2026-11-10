#ifndef CHARACTER_RENDERER_H
#define CHARACTER_RENDERER_H

#pragma once

#include <SFML/Graphics.hpp>

#include "character.h"
#include "character_model.h"

namespace Bomberman
{

// Draws a Character on top of a GridRenderer (use the same tile size) as a
// "bandana buccaneer": a round body, a knotted bandana with flapping tails,
// one big eye, an eyepatch and a small scar. While the character walks, it
// slides from the previous cell to the new one, hops and steps; dropping a
// bomb plays a short arms-up pose with a spark flashing where it landed.
//
// The artwork changes with Character::getFacing(): facing down shows the
// face; facing up shows the back of the head (no face, the bandana covers
// nearly all of it); facing left/right shows a side profile (a single
// centered arm/leg/ear, the visible eye and scar on the leading side, no
// eyepatch/strap since it is on the hidden cheek).
//
// Every shape is tessellated into one triangle vertex array rebuilt by
// update(), using the exact coordinates of the validated mock-ups (a 100x100
// view box with the body centered on (50, 56), radius 27), so the in-game
// character matches it at any resolution.
//
// The CharacterModel picks the look (validated mock-up "Équipage du
// galion"): Buccaneer (the original described above), Captain, Lookout,
// Parrot or Revenant; all share the same body template and animations.
class CharacterRenderer : public sf::Drawable, public sf::Transformable
{
public:
    // Only the bandana (knot, tails, dots) changes between players; skin and
    // outline stay the same so every crewmate reads the same way on screen.
    // The skin applies to the human models (Buccaneer, Captain, Lookout), the
    // bandana color to the Buccaneer only; the other colors are fixed per
    // model.
    struct Palette
    {
        sf::Color skin;
        sf::Color bandana;

        Palette() noexcept; // default crew colors, defined out of line
        Palette(sf::Color skinColor, sf::Color bandanaColor) noexcept : skin(skinColor), bandana(bandanaColor) {}
    };

    static constexpr int PlayerCount = 4;
    static constexpr float BombAnimDuration = 0.5f; // seconds, arms-up pose
    // Death animation "Carbonisé" (validated mock-up "Fin de l'abordage"):
    // a white flash, charred black and shaking, then crumbling into ashes
    // in a puff of grey smoke.
    static constexpr float DeathDuration = 1.3f; // seconds

    // =========================
    // Static Methods
    // =========================
    // One of the PlayerCount preset crew colors; out-of-range indices clamp.
    [[nodiscard]] static Palette paletteForPlayer(int playerIndex) noexcept;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit CharacterRenderer(float tileSize, Palette palette = Palette{},
                               CharacterModel model = CharacterModel::Buccaneer);

    // =========================
    // Public Methods
    // =========================
    // Rebuild the artwork from the character state; call it every frame.
    // deltaSeconds advances the bomb-drop pose started by triggerBombAnimation();
    // it can be left at 0 for tests that only check the idle/walk pose.
    void update(const Character& character, float deltaSeconds = 0.f);

    // Starts the short arms-up pose and ground spark; call it once a bomb has
    // actually been placed in front of the character.
    void triggerBombAnimation() noexcept;
    [[nodiscard]] bool isPlayingBombAnimation() const noexcept;

    // Starts the death animation (call it on the PlayerKilled event): the
    // following update()s play it on the character's last cell, whatever
    // the character itself does. Once finished, nothing is drawn any more.
    void triggerDeathAnimation() noexcept;
    // Started and not finished yet.
    [[nodiscard]] bool isDying() const noexcept;
    [[nodiscard]] bool isDeathAnimationFinished() const noexcept;

    // Center of the character's cell in pixels (slides between cells while
    // walking), relative to the grid origin.
    [[nodiscard]] sf::Vector2f getCenter() const noexcept;
    // Center of the round body: same as getCenter() but lifted while hopping.
    [[nodiscard]] sf::Vector2f getBodyCenter() const noexcept;
    [[nodiscard]] const sf::VertexArray& getVertices() const noexcept;

    [[nodiscard]] CharacterModel getModel() const noexcept;
    void setModel(CharacterModel model) noexcept; // applies at the next update()

    // Multiplies every color by brightness (clamped to [0, 1]; 1 = as
    // designed), e.g. to dim the characters that are not selected. Applies
    // at the next update().
    void setBrightness(float brightness) noexcept;
    [[nodiscard]] float getBrightness() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // Turns the freshly built character into the death animation's frame.
    void applyDeathAnimation();

    // =========================
    // Members
    // =========================
    float m_tileSize;
    Palette m_palette;
    CharacterModel m_model;
    float m_brightness = 1.f;
    sf::Vector2f m_center;
    sf::Vector2f m_bodyCenter;
    float m_bombAnimElapsed; // >= BombAnimDuration means idle
    float m_deathElapsed = -1.f; // < 0: alive
    float m_elapsed = 0.f;       // seconds, for idle animations (the spaniel's tail)
    sf::VertexArray m_vertices;
};

} // namespace Bomberman

#endif // CHARACTER_RENDERER_H

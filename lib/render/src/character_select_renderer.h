#ifndef CHARACTER_SELECT_RENDERER_H
#define CHARACTER_SELECT_RENDERER_H

#pragma once

#include <vector>

#include <SFML/Graphics.hpp>

#include "character_model.h"
#include "character_renderer.h"

namespace Bomberman
{

// The pirate selection screen shown at launch (validated mock-up "Choix du
// pirate", layout "Sur le pont"): the five crew models stand in a row on the
// galleon's deck; the selected one stands forward, bigger, hopping in a
// golden circle, the others stay back, dimmed; golden arrows frame the row
// and five dots at the bottom show the position. No text: the project ships
// no font.
//
// Everything is laid out in a DesignSize (1600x900) space; viewFor() fits it
// in any screen, centered, with black bars where the aspect ratio differs.
class CharacterSelectRenderer : public sf::Drawable
{
public:
    static constexpr sf::Vector2f DesignSize{1600.f, 900.f};
    static constexpr float DimmedBrightness = 0.6f; // colors of the models not selected
    static constexpr float HopHeight = 18.f;        // design units
    static constexpr float HopFrequency = 1.6f;     // hops per second: offset = |sin(t * pi * f)| * HopHeight

    // =========================
    // Constructors / Destructor
    // =========================
    CharacterSelectRenderer();

    // =========================
    // Static Methods
    // =========================
    // Index (0..CharacterModelCount-1) after moving by step, wrapping around.
    [[nodiscard]] static int wrap(int index, int step) noexcept;

    // View showing the whole design space, centered and scaled to fit.
    [[nodiscard]] static sf::View viewFor(sf::Vector2u screenSize);

    // =========================
    // Public Methods
    // =========================
    // Rebuild the screen for the selected model; deltaSeconds advances the
    // hop of the selected one.
    void update(CharacterModel selected, float deltaSeconds = 0.f);

    [[nodiscard]] CharacterModel getSelected() const noexcept;
    // Center of the selected model's body, in design units.
    [[nodiscard]] sf::Vector2f getSelectedCenter() const noexcept;
    // Floor, shadows, selection circle, arrows and dots.
    [[nodiscard]] const sf::VertexArray& getDecor() const noexcept;
    // One renderer per model, in AllCharacterModels order.
    [[nodiscard]] const std::vector<CharacterRenderer>& getCharacters() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // =========================
    // Members
    // =========================
    CharacterModel m_selected = CharacterModel::Buccaneer;
    float m_elapsed = 0.f;
    sf::Vector2f m_selectedCenter;
    sf::VertexArray m_back;  // floor and shadows, under every character
    sf::VertexArray m_ring;  // the selection circle, over the dimmed ones
    sf::VertexArray m_front; // arrows and dots, over everything
    std::vector<CharacterRenderer> m_characters;
};

} // namespace Bomberman

#endif // CHARACTER_SELECT_RENDERER_H

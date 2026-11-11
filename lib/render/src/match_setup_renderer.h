#ifndef MATCH_SETUP_RENDERER_H
#define MATCH_SETUP_RENDERER_H

#pragma once

#include <array>
#include <vector>

#include <SFML/Graphics.hpp>

#include "character_renderer.h"
#include "grid.h"
#include "grid_renderer.h"
#include "match_setup.h"

namespace Bomberman
{

// The launch menu (validated mock-up "Préparer l'abordage", layout "Le
// tableau de bord"): one wooden panel per setting on the galleon's deck,
// from top to bottom
//   - mode: solo against the computer (the buccaneer facing a skeleton),
//     host a network game (the captain's ship sending waves, a crown) or
//     join one (a spyglass and waves) (validated mock-up "Rassembler
//     l'équipage");
//   - pirate: the five crew models, the chosen one bigger, hopping in a
//     golden circle, the others dimmed;
//   - map: a real miniature of each map (GridRenderer drawn small);
//   - difficulty: one, two or three skulls on a green, orange or red
//     pennant;
//   - opponents: 1, 2 or 3 skeleton heads, each count in its own wooden
//     slot with as many dots under it;
// and a golden launch button (grey when the setup cannot start). The
// focused panel has a golden frame, a small golden badge at the start of
// each row says what it sets; the rows the mode does not use are shaded.
// No text: the project ships no font.
//
// Laid out in a DesignSize (1600x900) space; viewFor() fits it in any
// screen with black bars, like the rest of the menus.
class MatchSetupRenderer : public sf::Drawable
{
public:
    static constexpr sf::Vector2f DesignSize{1600.f, 900.f};
    static constexpr float DimmedBrightness = 0.6f; // what is not selected
    static constexpr float HopHeight = 18.f;        // design units, like the old selection screen
    static constexpr float HopFrequency = 1.6f;     // hops per second
    static constexpr float PulseFrequency = 0.8f;   // launch button, pulses per second
    // Where each map miniature fits (design units), centered.
    static constexpr sf::Vector2f MapSlotSize{195.f, 169.f};

    // =========================
    // Constructors / Destructor
    // =========================
    // maps: what the Map row offers, in order (their miniatures are built
    // once here).
    explicit MatchSetupRenderer(const std::vector<Grid>& maps = {});

    // =========================
    // Static Methods
    // =========================
    [[nodiscard]] static sf::View viewFor(sf::Vector2u screenSize);

    // Panel of a row, in design units.
    [[nodiscard]] static sf::FloatRect rowBounds(MatchSetup::Row row) noexcept;

    // =========================
    // Public Methods
    // =========================
    // Rebuilds the screen for setup; deltaSeconds advances the animations
    // (hop of the chosen pirate, pulse of the launch button).
    void update(const MatchSetup& setup, float deltaSeconds = 0.f);

    // Wooden decor under the characters (deck, panels, badges, frames,
    // pennants, slots).
    [[nodiscard]] const sf::VertexArray& getDecor() const noexcept;
    // Drawn over the dimmed characters (swords, crown and waves of the mode
    // icons, golden circle, dots, frames of the opponents' slots).
    [[nodiscard]] const sf::VertexArray& getOverlay() const noexcept;
    // Drawn over the map miniatures (dimming, frames, shaded rows, launch
    // button).
    [[nodiscard]] const sf::VertexArray& getFront() const noexcept;

    // Every character shown except the chosen pirate (mode icons, badges,
    // the other pirates, opponents' heads), then the chosen pirate last.
    [[nodiscard]] const std::vector<CharacterRenderer>& getCharacters() const noexcept;
    // Body center of the chosen pirate, in design units.
    [[nodiscard]] sf::Vector2f getSelectedPirateCenter() const noexcept;
    [[nodiscard]] const std::vector<GridRenderer>& getMaps() const noexcept;
    [[nodiscard]] bool isLaunchEnabled() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // A character of the crew whose body (mock-up view box of size units)
    // is centered on center.
    void place(CharacterModel model, sf::Vector2f center, float size, float brightness,
               CharacterRenderer::Palette palette = CharacterRenderer::Palette{});

    // =========================
    // Members
    // =========================
    float m_elapsed = 0.f;
    bool m_launchEnabled = false;
    int m_firstMap = 0; // first map of the page shown
    sf::Vector2f m_selectedCenter;
    sf::VertexArray m_decor;
    sf::VertexArray m_overlay;
    sf::VertexArray m_front;
    std::vector<CharacterRenderer> m_characters; // the chosen pirate is the last one
    std::vector<GridRenderer> m_maps;
};

} // namespace Bomberman

#endif // MATCH_SETUP_RENDERER_H

#ifndef DISCOVERY_RENDERER_H
#define DISCOVERY_RENDERER_H

#pragma once

#include <map>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "character_model.h"
#include "character_renderer.h"
#include "grid.h"
#include "grid_renderer.h"

namespace Bomberman
{

// A network game heard on the local network, as the search screen shows it.
struct DiscoveryCard
{
    std::string key; // identifies the host across frames (its address and port)
    CharacterModel hostModel = CharacterModel::Buccaneer;
    int seatsTaken = 1;
    int seatCount = 4;
    Grid map;
};

// "Chercher un navire" (validated mock-up "Rassembler l'équipage"), the
// screen of a client looking for a game: a night sky over the deck, a
// spyglass sweeping the sea from its tripod, and one card per host heard
// (CardsPerPage at a time): the host's pirate on its ship under a crown,
// the miniature of its map and one dot per seat (colored when taken,
// dashed when free). The selected card has a golden frame, the others are
// dimmed. Waiting dots show the search goes on; with no host at all the
// spyglass stays alone with bigger waiting dots. Same 1600x900 design
// space as the menus.
class DiscoveryRenderer : public sf::Drawable
{
public:
    static constexpr sf::Vector2f DesignSize{1600.f, 900.f};
    static constexpr int CardsPerPage = 3;
    static constexpr sf::Vector2f MapSlotSize{195.f, 169.f};

    // =========================
    // Constructors / Destructor
    // =========================
    DiscoveryRenderer();

    // =========================
    // Static Methods
    // =========================
    [[nodiscard]] static sf::View viewFor(sf::Vector2u screenSize);
    // The card of slot (0..CardsPerPage-1), in design units.
    [[nodiscard]] static sf::FloatRect cardBounds(int slot) noexcept;

    // =========================
    // Public Methods
    // =========================
    // selected: index in cards (clamped). Miniatures are only rebuilt for
    // hosts not seen before.
    void update(const std::vector<DiscoveryCard>& cards, int selected, float deltaSeconds = 0.f);

    [[nodiscard]] const sf::VertexArray& getDecor() const noexcept;
    [[nodiscard]] const sf::VertexArray& getFront() const noexcept;
    [[nodiscard]] const std::vector<CharacterRenderer>& getCharacters() const noexcept;
    // Miniatures of the cards shown, in slot order.
    [[nodiscard]] std::vector<const GridRenderer*> getShownMaps() const;
    [[nodiscard]] int getCardsShown() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // =========================
    // Members
    // =========================
    float m_elapsed = 0.f;
    sf::VertexArray m_decor;
    sf::VertexArray m_front;
    std::vector<CharacterRenderer> m_characters;
    std::map<std::string, GridRenderer> m_miniatures;
    std::vector<std::string> m_shown; // keys of the cards shown
};

} // namespace Bomberman

#endif // DISCOVERY_RENDERER_H

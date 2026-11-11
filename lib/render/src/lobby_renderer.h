#ifndef LOBBY_RENDERER_H
#define LOBBY_RENDERER_H

#pragma once

#include <array>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

#include "character_model.h"
#include "character_renderer.h"
#include "grid.h"
#include "grid_renderer.h"

namespace Bomberman
{

// What the waiting room shows (filled by exe/main.cpp from the host's or
// the client's session): the crew model on each seat (nothing when free),
// which seat is this computer's, and whether this computer is the host.
struct LobbyView
{
    static constexpr int SeatCount = 4;

    std::array<std::optional<CharacterModel>, SeatCount> seats{};
    int localSeat = 0;
    bool isHost = true;
    bool canStart = false; // the host's launch button (2 players or more)
};

// The waiting room of a network game (validated mock-up "Rassembler
// l'équipage", proposal A "Le pont d'embarquement"), in the same 1600x900
// design space as the launch menu (viewFor() fits it with black bars):
//   - four seat panels side by side, each under a pennant of its color (red,
//     blue, green, gold: CharacterRenderer::paletteForPlayer(seat)) with 1
//     to 4 dots; the captain's crown over seat 0;
//   - a taken seat shows its pirate standing on a ring of the seat color;
//     this computer's seat has a golden frame, its pirate hops between two
//     arrows (Q/D change it); a pirate who just joined pops in;
//   - a free seat shows the dashed outline of a body and waiting dots;
//   - at the bottom: the map's miniature, the network waves, four lanterns
//     (lit for each player) and the launch button (host) or an hourglass
//     (clients wait for the host).
// No text: the project ships no font.
class LobbyRenderer : public sf::Drawable
{
public:
    static constexpr sf::Vector2f DesignSize{1600.f, 900.f};
    static constexpr sf::Vector2f MapSlotSize{195.f, 169.f};
    static constexpr float PopDuration = 0.35f; // a newcomer grows from 20 % to full size
    static constexpr float SeatPirateSize = 180.f;
    static constexpr float LocalPirateSize = 210.f;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit LobbyRenderer(const Grid& map = Grid());

    // =========================
    // Static Methods
    // =========================
    [[nodiscard]] static sf::View viewFor(sf::Vector2u screenSize);
    // Panel of seat (0..3), in design units.
    [[nodiscard]] static sf::FloatRect seatBounds(int seat) noexcept;

    // =========================
    // Public Methods
    // =========================
    // The map the room is about (its miniature is rebuilt).
    void setMap(const Grid& map);

    // Rebuilds the screen; deltaSeconds advances the animations.
    void update(const LobbyView& view, float deltaSeconds = 0.f);

    [[nodiscard]] const sf::VertexArray& getDecor() const noexcept;
    [[nodiscard]] const sf::VertexArray& getFront() const noexcept;
    // The seated pirates, by seat order.
    [[nodiscard]] const std::vector<CharacterRenderer>& getCharacters() const noexcept;
    [[nodiscard]] const GridRenderer& getMap() const noexcept;
    [[nodiscard]] bool isLaunchEnabled() const noexcept;
    // Body center of the pirate on seat (design units), if taken.
    [[nodiscard]] std::optional<sf::Vector2f> getPirateCenter(int seat) const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void place(CharacterModel model, sf::Vector2f center, float size, int seat);

    // =========================
    // Members
    // =========================
    float m_elapsed = 0.f;
    bool m_launchEnabled = false;
    std::array<std::optional<CharacterModel>, LobbyView::SeatCount> m_previousSeats{};
    std::array<float, LobbyView::SeatCount> m_seatedFor{}; // seconds since the seat was taken
    std::array<std::optional<sf::Vector2f>, LobbyView::SeatCount> m_pirateCenters{};
    sf::VertexArray m_decor;
    sf::VertexArray m_front;
    std::vector<CharacterRenderer> m_characters;
    GridRenderer m_map;
};

} // namespace Bomberman

#endif // LOBBY_RENDERER_H

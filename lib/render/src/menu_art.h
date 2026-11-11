#ifndef MENU_ART_H
#define MENU_ART_H

#pragma once

#include <array>
#include <vector>

#include <SFML/Graphics.hpp>

#include "mesh.h"

namespace Bomberman
{

// Drawings shared by the menu screens (launch menu, waiting room, search
// for a ship), in the 1600x900 design space of the validated mock-ups
// ("Préparer l'abordage", "Rassembler l'équipage"). Each function appends
// triangles to a Mesh, with the mock-up's coordinates and colors; thin
// strokes are always opaque (see CLAUDE.md), callers pass the opaque color
// a translucent mock-up ink gives over their background.
namespace MenuArt
{

constexpr float Pi = 3.14159265f;

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color Gold(212, 169, 74);
constexpr sf::Color GoldGlow(212, 169, 74, 89);
constexpr sf::Color Deck(196, 144, 90);
constexpr sf::Color Seam(70, 40, 18, 115);
constexpr sf::Color Bone(243, 234, 220);
constexpr sf::Color PanelFill(40, 22, 10, 82);
constexpr sf::Color PanelFocusedFill(40, 22, 10, 140);
constexpr sf::Color PanelEdge(95, 70, 44); // ink at 60 % over the deck
constexpr sf::Color FrameEdge(63, 46, 28); // ink at 70 % over a panel
constexpr sf::Color LaunchOff(109, 106, 99);
constexpr sf::Color Wood(107, 68, 38);
constexpr sf::Color DarkWood(91, 57, 32);
constexpr sf::Color BadgeFill(42, 26, 13);
// One color per seat of a network game: the bandanas of
// CharacterRenderer::paletteForPlayer(0..3) (red, blue, green, gold).
constexpr std::array<sf::Color, 4> SeatColors = {sf::Color(195, 59, 59), sf::Color(47, 111, 174),
                                                 sf::Color(58, 138, 78), sf::Color(201, 146, 47)};

// The mock-ups' filter: brightness(b).
[[nodiscard]] sf::Color dimmed(sf::Color color, float brightness);

// Points turned by angle (radians) around pivot.
[[nodiscard]] std::vector<sf::Vector2f> turned(std::vector<sf::Vector2f> points, sf::Vector2f pivot, float angle);

void roundedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Color fill);
void roundedFrame(Mesh& mesh, float x, float y, float w, float h, float r, float width, sf::Color color);
// SVG stroke-dasharray="dash gap" along a closed outline.
void dashedStroke(Mesh& mesh, const std::vector<sf::Vector2f>& closed, float width, float dash, float gap,
                  sf::Color color);

// Deck planks covering the whole design space.
void deckFloor(Mesh& mesh, sf::Vector2f size);

// Golden play button pulsing at frequency (grey and still when disabled).
void launchButton(Mesh& mesh, sf::Vector2f center, float radius, bool enabled, float elapsed, float frequency);

// Gold triangle pointing left (direction -1) or right (1).
void arrow(Mesh& mesh, sf::Vector2f center, float direction, float size);

// The captain's crown (the host's seat), size s.
void crown(Mesh& mesh, sf::Vector2f center, float s, float brightness = 1.f);

// Waiting for the captain: an hourglass whose sand runs for 1.7 s, then it
// turns over in 0.3 s.
void hourglass(Mesh& mesh, sf::Vector2f center, float s, float elapsed);

// Network waves: a dot and three arcs lighting up one after the other
// (lit/unlit colors given opaque); animate = false keeps them all lit.
void waves(Mesh& mesh, sf::Vector2f center, float s, float elapsed, sf::Color lit, sf::Color unlit,
           bool animate = true);

// A ship (hull, mast, sail, flag) in a 100x100 box of size s centered on
// center.
void ship(Mesh& mesh, sf::Vector2f center, float s, float brightness = 1.f);

// A spyglass of size s turned by degrees around center.
void spyglass(Mesh& mesh, sf::Vector2f center, float s, float degrees, float brightness = 1.f);

// Three dots pulsing one after the other (waiting).
void waitingDots(Mesh& mesh, sf::Vector2f center, float elapsed, float radius, float gap);

// count small bone dots in a row (a seat number).
void seatDots(Mesh& mesh, int count, sf::Vector2f center, float radius, float gap);

// An empty seat: the dashed outline of the crew's body template (mock-up
// view box of size units centered on center); edge = the opaque dash color.
void ghost(Mesh& mesh, sf::Vector2f center, float size, sf::Color edge);

} // namespace MenuArt

} // namespace Bomberman

#endif // MENU_ART_H

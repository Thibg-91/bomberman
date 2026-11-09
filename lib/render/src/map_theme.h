#ifndef MAP_THEME_H
#define MAP_THEME_H

#pragma once

#include <string>

namespace Bomberman
{

// Look of a map: floor, border and pillar artwork. The destructible crate is
// shared by every theme.
enum class MapTheme
{
    Deck,   // galleon deck: planks, bulwark, iron-banded masts
    Island, // treasure island: sand, sea with foam, rocks
    Cave,   // smugglers' cave: flagstones, rock, torches
    Docks   // harbor docks: weathered planks over water, barrels
};

// "deck", "island", "cave", "docks"; anything else (including an empty name)
// falls back to Deck so older level files keep working.
[[nodiscard]] MapTheme mapThemeFromName(const std::string& name) noexcept;

} // namespace Bomberman

#endif // MAP_THEME_H

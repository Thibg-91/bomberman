#include "map_theme.h"

namespace Bomberman
{

MapTheme mapThemeFromName(const std::string& name) noexcept
{
    if (name == "island")
        return MapTheme::Island;
    if (name == "cave")
        return MapTheme::Cave;
    if (name == "docks")
        return MapTheme::Docks;
    return MapTheme::Deck;
}

} // namespace Bomberman

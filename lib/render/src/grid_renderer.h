#ifndef GRID_RENDERER_H
#define GRID_RENDERER_H

#pragma once

#include <SFML/Graphics.hpp>

#include "grid.h"
#include "map_theme.h"

namespace Bomberman
{

// Draws a Grid with the artwork of a MapTheme: textured floors, a light grid
// over the playable cells, themed border/pillar walls and the shared cargo
// crate for destructible blocks. Everything is tessellated into one triangle
// vertex array (no texture, no GL needed to build it), so the whole board is a
// single draw call. Call update() whenever the grid content changes (e.g. a
// block got destroyed). Entities (players, bombs...) are drawn by their own
// renderers on top of it, using the same tile size.
class GridRenderer : public sf::Drawable, public sf::Transformable
{
public:
    static constexpr float DefaultTileSize = 48.f;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit GridRenderer(float tileSize = DefaultTileSize, MapTheme theme = MapTheme::Deck);

    // =========================
    // Public Methods
    // =========================
    void update(const Grid& grid);

    [[nodiscard]] float getTileSize() const noexcept;
    [[nodiscard]] MapTheme getTheme() const noexcept;
    [[nodiscard]] sf::Vector2u getPixelSize() const noexcept;
    [[nodiscard]] const sf::VertexArray& getVertices() const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // =========================
    // Members
    // =========================
    float m_tileSize;
    MapTheme m_theme;
    sf::Vector2u m_pixelSize;
    sf::VertexArray m_vertices;
};

} // namespace Bomberman

#endif // GRID_RENDERER_H

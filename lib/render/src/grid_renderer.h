#ifndef GRID_RENDERER_H
#define GRID_RENDERER_H

#pragma once

#include <SFML/Graphics.hpp>

#include "grid.h"

namespace Bomberman
{

// Draws a Grid as one colored square per tile, batched in a single vertex
// array. Call update() whenever the grid content changes (e.g. a block got
// destroyed). Entities (players, bombs...) are expected to be drawn by their
// own renderers on top of it, using the same tile size.
class GridRenderer : public sf::Drawable, public sf::Transformable
{
public:
    static constexpr float DefaultTileSize = 48.f;
    static constexpr float TileSpacing = 1.f;
    static constexpr std::size_t VerticesPerTile = 6;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit GridRenderer(float tileSize = DefaultTileSize);

    // =========================
    // Public Methods
    // =========================
    void update(const Grid& grid);

    [[nodiscard]] float getTileSize() const noexcept;
    [[nodiscard]] sf::Vector2u getPixelSize() const noexcept;
    [[nodiscard]] const sf::VertexArray& getVertices() const noexcept;

    // =========================
    // Static Methods
    // =========================
    [[nodiscard]] static sf::Color colorOf(TileType type) noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    // =========================
    // Members
    // =========================
    float m_tileSize;
    sf::Vector2u m_pixelSize;
    sf::VertexArray m_vertices;
};

} // namespace Bomberman

#endif // GRID_RENDERER_H

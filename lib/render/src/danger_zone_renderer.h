#ifndef DANGER_ZONE_RENDERER_H
#define DANGER_ZONE_RENDERER_H

#pragma once

#include <vector>

#include <SFML/Graphics.hpp>

#include "bomb.h"
#include "grid.h"

namespace Bomberman
{

// Tints in red the cells each ticking bomb is going to hit (its Blast on the
// current grid: the same cells as the explosion to come), more and more
// strongly and with a faster pulse as the fuse burns down. Drawn over the
// grid, under upgrades, bombs and characters (same tile size as the
// GridRenderer). Coordinates follow the validated mock-up: in each cell a
// 90x90 rounded square (radius 10) inset by 5, out of 100 units.
class DangerZoneRenderer : public sf::Drawable, public sf::Transformable
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    explicit DangerZoneRenderer(float tileSize);

    // =========================
    // Public Methods
    // =========================
    // Rebuild the tint from the bombs on the grid; call it every frame.
    void update(const std::vector<Bomb>& bombs, const Grid& grid);

    // Opacity (0-1) of the tint for a bomb, from how far its fuse has burnt.
    [[nodiscard]] static float tintOpacity(const Bomb& bomb) noexcept;

    [[nodiscard]] float getTileSize() const noexcept;
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
    sf::VertexArray m_vertices;
};

} // namespace Bomberman

#endif // DANGER_ZONE_RENDERER_H

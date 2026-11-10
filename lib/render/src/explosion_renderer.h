#ifndef EXPLOSION_RENDERER_H
#define EXPLOSION_RENDERER_H

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <SFML/Graphics.hpp>

#include "blast.h"

namespace Bomberman
{

// Draws bomb explosions as a cross covering exactly the cells of their
// Blast (so its length follows the bomb's blast radius), on top of a
// GridRenderer (same tile size). Two looks from the validated mock-up
// ("Souffle des bombes"):
//   - Fireballs (used in game): a chain of fireballs popping cell after cell
//     from the center, which turn to grey smoke before fading out;
//   - FlameCross (kept for a future feature): a continuous four-layer flame
//     cross with round ends, shooting out from the center.
//
// Coordinates are the mock-up's 100 units per cell. Each explosion lasts
// Duration seconds; it is tessellated into a range of one vertex array
// (opaque colors), and its fade-out is applied to the explosion as a whole
// when drawing (through an off-screen texture), like the mock-up's group
// opacity: overlapping shapes never show through each other.
class ExplosionRenderer : public sf::Drawable, public sf::Transformable
{
public:
    enum class Style
    {
        Fireballs,
        FlameCross
    };

    static constexpr float Duration = 0.6f; // seconds

    // =========================
    // Constructors / Destructor
    // =========================
    explicit ExplosionRenderer(float tileSize, Style style = Style::Fireballs);

    // =========================
    // Public Methods
    // =========================
    // Starts drawing an explosion (e.g. on GameEvent::Type::Explosion).
    void add(const Blast& blast);

    // Ages the explosions, drops the finished ones and rebuilds the artwork;
    // call it every frame.
    void update(float deltaSeconds);

    [[nodiscard]] Style getStyle() const noexcept;
    [[nodiscard]] float getTileSize() const noexcept;
    [[nodiscard]] std::size_t getActiveCount() const noexcept;
    // Opacity of the index-th active explosion: 1, then down to 0 at the end.
    [[nodiscard]] float getOpacity(std::size_t index) const;
    [[nodiscard]] const sf::VertexArray& getVertices() const noexcept;

private:
    struct Active
    {
        Blast blast;
        float elapsed = 0.f;
        std::size_t firstVertex = 0;
        std::size_t vertexCount = 0;
        float opacity = 1.f;
    };

    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    static void drawFadedVertices(sf::RenderTarget& target, const sf::RenderStates& states,
                                  const sf::Vertex* vertices, std::size_t count, float opacity);

    // =========================
    // Members
    // =========================
    float m_tileSize;
    Style m_style;
    std::vector<Active> m_explosions;
    sf::VertexArray m_vertices;
    mutable std::unique_ptr<sf::RenderTexture> m_canvas; // created on first fading draw
};

} // namespace Bomberman

#endif // EXPLOSION_RENDERER_H

#ifndef UPGRADE_RENDERER_H
#define UPGRADE_RENDERER_H

#pragma once

#include <vector>

#include <SFML/Graphics.hpp>

#include "upgrade.h"

namespace Bomberman
{

// Draws the upgrades lying on the grid, on top of a GridRenderer (use the
// same tile size) and under the bombs and characters. Each one floats over
// its own shadow, bobbing gently, with a twinkling star:
//   - ExtraBomb: a small lit bomb with a golden "+" badge;
//   - PushStrength: a flexed, tattooed arm (the crew's skin and red
//     bandana wristband);
//   - BlastRadius: a cartoon explosion burst.
//
// Every shape uses the exact coordinates of the validated mock-up (a 100x100
// view box per cell; the item is drawn at 82 % around (9, 5) and bobs up by
// up to BobAmplitude units), tessellated into one triangle vertex array
// rebuilt by update(): one draw call for all upgrades.
class UpgradeRenderer : public sf::Drawable, public sf::Transformable
{
public:
    static constexpr float AnimationPeriod = 1.6f; // seconds per bob / twinkle
    static constexpr float BobAmplitude = 3.5f;    // mock units (1/100 of a tile)

    // =========================
    // Constructors / Destructor
    // =========================
    explicit UpgradeRenderer(float tileSize);

    // =========================
    // Public Methods
    // =========================
    // Rebuild the artwork; call it every frame. deltaSeconds advances the
    // bobbing/twinkling animation (0 keeps it where it is).
    void update(const std::vector<Upgrade>& upgrades, float deltaSeconds = 0.f);

    [[nodiscard]] float getTileSize() const noexcept;
    // Seconds of animation played so far.
    [[nodiscard]] float getElapsed() const noexcept;
    // How far the items are lifted right now, in [0, BobAmplitude] mock units.
    [[nodiscard]] float getBobOffset() const noexcept;
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
    float m_elapsed = 0.f;
    sf::VertexArray m_vertices;
};

} // namespace Bomberman

#endif // UPGRADE_RENDERER_H

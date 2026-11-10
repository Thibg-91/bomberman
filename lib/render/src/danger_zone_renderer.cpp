#include "danger_zone_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "blast.h"
#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr sf::Color Danger(214, 60, 40);

} // namespace

// =========================
// Constructors / Destructor
// =========================
DangerZoneRenderer::DangerZoneRenderer(float tileSize)
    : m_tileSize(tileSize),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void DangerZoneRenderer::update(const std::vector<Bomb>& bombs, const Grid& grid)
{
    const float unit = m_tileSize / 100.f;
    m_vertices.clear();
    Mesh mesh(m_vertices);
    for (const Bomb& bomb : bombs)
    {
        if (bomb.hasExploded())
            continue;

        sf::Color tint = Danger;
        tint.a = static_cast<std::uint8_t>(std::lround(tintOpacity(bomb) * 255.f));
        for (const Position cell : Blast::compute(bomb.getPosition(), bomb.getBlastRadius(), grid).cells())
        {
            const float x = static_cast<float>(cell.x) * m_tileSize;
            const float y = static_cast<float>(cell.y) * m_tileSize;
            mesh.polygon(roundedRectPoints(x + 5.f * unit, y + 5.f * unit, 90.f * unit, 90.f * unit, 10.f * unit),
                         tint);
        }
    }
}

float DangerZoneRenderer::tintOpacity(const Bomb& bomb) noexcept
{
    const float fuse = bomb.getFuseDuration();
    const float progress = fuse > 0.f ? 1.f - std::clamp(bomb.getTimeRemaining() / fuse, 0.f, 1.f) : 1.f;
    const float elapsed = fuse - bomb.getTimeRemaining();
    // Stronger and pulsing faster as the explosion gets close.
    const float pulse = 0.5f + 0.5f * std::sin(elapsed * (4.f + progress * 14.f));
    return 0.10f + 0.22f * progress + 0.08f * pulse;
}

float DangerZoneRenderer::getTileSize() const noexcept
{
    return m_tileSize;
}

const sf::VertexArray& DangerZoneRenderer::getVertices() const noexcept
{
    return m_vertices;
}

// =========================
// Private Methods
// =========================
void DangerZoneRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

#include "grid_renderer.h"

#include <cmath>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
GridRenderer::GridRenderer(float tileSize)
    : m_tileSize(tileSize),
      m_pixelSize(0u, 0u),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void GridRenderer::update(const Grid& grid)
{
    const auto tileCount = static_cast<std::size_t>(grid.getWidth()) * static_cast<std::size_t>(grid.getHeight());
    m_vertices.resize(tileCount * VerticesPerTile);
    m_pixelSize = {static_cast<unsigned int>(std::lround(grid.getWidth() * m_tileSize)),
                   static_cast<unsigned int>(std::lround(grid.getHeight() * m_tileSize))};

    // Shrinking each square a bit lets the background show through as grid lines.
    const float size = m_tileSize - TileSpacing;

    std::size_t index = 0;
    for (int y = 0; y < grid.getHeight(); ++y)
    {
        for (int x = 0; x < grid.getWidth(); ++x)
        {
            const sf::Color color = colorOf(grid.getTile({x, y}).getType());
            const sf::Vector2f topLeft(x * m_tileSize, y * m_tileSize);
            const sf::Vector2f topRight = topLeft + sf::Vector2f(size, 0.f);
            const sf::Vector2f bottomLeft = topLeft + sf::Vector2f(0.f, size);
            const sf::Vector2f bottomRight = topLeft + sf::Vector2f(size, size);

            for (const sf::Vector2f& corner : {topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight})
            {
                m_vertices[index].position = corner;
                m_vertices[index].color = color;
                ++index;
            }
        }
    }
}

float GridRenderer::getTileSize() const noexcept
{
    return m_tileSize;
}

sf::Vector2u GridRenderer::getPixelSize() const noexcept
{
    return m_pixelSize;
}

const sf::VertexArray& GridRenderer::getVertices() const noexcept
{
    return m_vertices;
}

// =========================
// Static Methods
// =========================
sf::Color GridRenderer::colorOf(TileType type) noexcept
{
    switch (type)
    {
    case TileType::Wall:
        return sf::Color(90, 90, 100);
    case TileType::DestructibleBlock:
        return sf::Color(170, 110, 60);
    case TileType::Empty:
        break;
    }
    return sf::Color(40, 130, 60);
}

// =========================
// Private Methods
// =========================
void GridRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

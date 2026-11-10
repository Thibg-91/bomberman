#include "character_select_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "character.h"
#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color Deck(196, 144, 90);
constexpr sf::Color Seam(70, 40, 18, 115);
constexpr sf::Color Shadow(0, 0, 0, 64);
constexpr sf::Color Gold(212, 169, 74);
constexpr sf::Color GoldGlow(212, 169, 74, 89);
constexpr sf::Color DotOff(255, 255, 255, 89);

// Mock-up layout (design units).
constexpr std::array<float, CharacterModelCount> SlotX = {230.f, 458.f, 686.f, 914.f, 1142.f, 1370.f};
constexpr float DimmedY = 470.f;
constexpr float DimmedSize = 200.f; // mock-up view box (100 units) drawn this big
constexpr float SelectedY = 520.f;
constexpr float SelectedSize = 300.f;

int indexOf(CharacterModel model)
{
    const auto it = std::find(AllCharacterModels.begin(), AllCharacterModels.end(), model);
    return static_cast<int>(it - AllCharacterModels.begin());
}

// Arrow pointing towards dir (-1 left, +1 right): the mock-up's triangle,
// gold with a rounded ink outline.
void arrow(Mesh& mesh, sf::Vector2f c, float dir, float size = 34.f)
{
    const std::vector<sf::Vector2f> points = {{c.x + dir * size, c.y},
                                              {c.x - dir * size * 0.6f, c.y - size},
                                              {c.x - dir * size * 0.6f, c.y + size}};
    mesh.polygon(points, Gold);
    mesh.stroke(points, 6.f, Ink);
}

// A CharacterRenderer draws its character on cell (0, 0): pick the tile
// size giving a body as big as the mock-up (body radius 27 units of a
// size-wide view box) and move it so the body center lands on center.
CharacterRenderer placed(CharacterModel model, sf::Vector2f center, float size, float brightness)
{
    const float tileSize = size / 100.f * 27.f / 0.33f;
    CharacterRenderer renderer(tileSize, CharacterRenderer::Palette{}, model);
    renderer.setBrightness(brightness);
    renderer.update(Character({0, 0}));
    renderer.setPosition(center - sf::Vector2f(tileSize / 2.f, tileSize / 2.f));
    return renderer;
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
CharacterSelectRenderer::CharacterSelectRenderer()
    : m_back(sf::PrimitiveType::Triangles),
      m_ring(sf::PrimitiveType::Triangles),
      m_front(sf::PrimitiveType::Triangles)
{
    update(m_selected);
}

// =========================
// Static Methods
// =========================
int CharacterSelectRenderer::wrap(int index, int step) noexcept
{
    return ((index + step) % CharacterModelCount + CharacterModelCount) % CharacterModelCount;
}

sf::View CharacterSelectRenderer::viewFor(sf::Vector2u screenSize)
{
    const sf::Vector2f screen(static_cast<float>(std::max(screenSize.x, 1u)),
                              static_cast<float>(std::max(screenSize.y, 1u)));
    const float scale = std::min(screen.x / DesignSize.x, screen.y / DesignSize.y);
    return sf::View(DesignSize / 2.f, screen / scale);
}

// =========================
// Public Methods
// =========================
void CharacterSelectRenderer::update(CharacterModel selected, float deltaSeconds)
{
    m_selected = selected;
    if (deltaSeconds > 0.f)
        m_elapsed = std::fmod(m_elapsed + deltaSeconds, 10.f); // 10 s = 16 whole hops: seamless wrap
    const int selectedIndex = indexOf(selected);

    // Deck planks.
    m_back.clear();
    Mesh back(m_back);
    back.rect(0.f, 0.f, DesignSize.x, DesignSize.y, Deck);
    for (float y = 60.f; y < DesignSize.y; y += 60.f)
        back.rect(0.f, y - 2.f, DesignSize.x, 3.f, Seam);

    // The selected one stands forward, bigger and hopping; the others stay
    // back, dimmed, each over its shadow.
    const float x = SlotX[static_cast<std::size_t>(selectedIndex)];
    const float hop = -std::abs(std::sin(m_elapsed * Pi * HopFrequency)) * HopHeight;
    m_selectedCenter = {x, SelectedY + hop};
    m_characters.clear();
    for (int i = 0; i < CharacterModelCount; ++i)
    {
        const std::size_t slot = static_cast<std::size_t>(i);
        if (i == selectedIndex)
        {
            m_characters.push_back(placed(selected, m_selectedCenter, SelectedSize, 1.f));
            continue;
        }
        back.ellipse({SlotX[slot], 560.f}, 70.f, 16.f, Shadow);
        m_characters.push_back(placed(AllCharacterModels[slot], {SlotX[slot], DimmedY}, DimmedSize, DimmedBrightness));
    }

    // Golden circle under the selected one.
    m_ring.clear();
    Mesh ring(m_ring);
    ring.ellipse({x, 640.f}, 140.f, 34.f, GoldGlow);
    ring.stroke(ellipsePoints({x, 640.f}, 140.f, 34.f, 0.f, Mesh::segmentsFor(140.f)), 6.f, Gold);

    // Arrows and position dots.
    m_front.clear();
    Mesh front(m_front);
    arrow(front, {80.f, 470.f}, -1.f);
    arrow(front, {DesignSize.x - 80.f, 470.f}, 1.f);
    for (int i = 0; i < CharacterModelCount; ++i)
    {
        const bool on = i == selectedIndex;
        const sf::Vector2f c(DesignSize.x / 2.f + (static_cast<float>(i) - static_cast<float>(CharacterModelCount - 1) / 2.f) * 34.f,
                             820.f);
        const float r = on ? 11.f : 8.f;
        front.circle(c, r, on ? Gold : DotOff);
        front.ring(c, r, 4.f, Ink);
    }
}

CharacterModel CharacterSelectRenderer::getSelected() const noexcept
{
    return m_selected;
}

sf::Vector2f CharacterSelectRenderer::getSelectedCenter() const noexcept
{
    return m_selectedCenter;
}

const sf::VertexArray& CharacterSelectRenderer::getDecor() const noexcept
{
    return m_back;
}

const std::vector<CharacterRenderer>& CharacterSelectRenderer::getCharacters() const noexcept
{
    return m_characters;
}

// =========================
// Private Methods
// =========================
void CharacterSelectRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    const int selectedIndex = indexOf(m_selected);
    target.draw(m_back, states);
    for (std::size_t i = 0; i < m_characters.size(); ++i)
        if (static_cast<int>(i) != selectedIndex)
            target.draw(m_characters[i], states);
    target.draw(m_ring, states);
    target.draw(m_characters[static_cast<std::size_t>(selectedIndex)], states);
    target.draw(m_front, states);
}

} // namespace Bomberman

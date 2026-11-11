#include "discovery_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "character.h"
#include "map_theme.h"
#include "match_setup_renderer.h"
#include "menu_art.h"
#include "mesh.h"

namespace Bomberman
{

namespace
{

using namespace MenuArt;

// Mock-up layout (design units).
constexpr float CardX = 480.f;
constexpr float CardStep = 360.f;
constexpr float CardY = 120.f;
constexpr float CardWidth = 320.f;
constexpr float CardHeight = 600.f;
constexpr sf::Vector2f SpyglassCenter{250.f, 590.f};
constexpr float ConeLength = 1400.f;
constexpr int StarCount = 40;

// Mock-up colors (thin strokes opaque over their backgrounds).
constexpr sf::Color Sky(15, 42, 58);
constexpr sf::Color DeckStrip(138, 90, 46);
constexpr sf::Color Light(255, 236, 180, 18);
constexpr sf::Color CardOn(40, 22, 10, 191);
constexpr sf::Color CardOff(40, 22, 10, 128);
constexpr sf::Color CardEdgeOff(72, 90, 99); // bone at 25 % over the sky
constexpr sf::Color FreeSeat(0, 0, 0, 77);
constexpr sf::Color FreeSeatEdgeOn(112, 105, 97);  // bone at 40 % over a free seat of the selected card
constexpr sf::Color FreeSeatEdgeOff(109, 107, 103); // ... of another card
constexpr sf::Color MapDimming(0, 0, 0, 102);       // brightness(.6)
constexpr float DimmedBrightness = 0.6f;
constexpr float HopHeight = 18.f;
constexpr float HopFrequency = 1.6f;

} // namespace

// =========================
// Constructors / Destructor
// =========================
DiscoveryRenderer::DiscoveryRenderer()
    : m_decor(sf::PrimitiveType::Triangles),
      m_front(sf::PrimitiveType::Triangles)
{
    update({}, 0);
}

// =========================
// Static Methods
// =========================
sf::View DiscoveryRenderer::viewFor(sf::Vector2u screenSize)
{
    return MatchSetupRenderer::viewFor(screenSize);
}

sf::FloatRect DiscoveryRenderer::cardBounds(int slot) noexcept
{
    return {{CardX + static_cast<float>(slot) * CardStep, CardY}, {CardWidth, CardHeight}};
}

// =========================
// Public Methods
// =========================
void DiscoveryRenderer::update(const std::vector<DiscoveryCard>& cards, int selected, float deltaSeconds)
{
    if (deltaSeconds > 0.f)
        m_elapsed = std::fmod(m_elapsed + deltaSeconds, 100.f * Pi); // sweeps, stars and dots all loop by then
    m_decor.clear();
    m_front.clear();
    m_characters.clear();
    m_shown.clear();
    Mesh decor(m_decor);
    Mesh front(m_front);
    const float t = m_elapsed;

    // Night sky, stars, the deck.
    decor.rect(0.f, 0.f, DesignSize.x, DesignSize.y, Sky);
    for (int k = 0; k < StarCount; ++k)
    {
        const float x = static_cast<float>((k * 397) % 1600);
        const float y = static_cast<float>((k * 211) % 380);
        const float alpha = std::max(0.f, 0.3f + 0.3f * std::sin(t * 2.f + static_cast<float>(k)));
        decor.circle({x, y}, k % 3 != 0 ? 2.f : 3.f,
                     sf::Color(Bone.r, Bone.g, Bone.b, static_cast<std::uint8_t>(alpha * 255.f)));
    }
    decor.rect(0.f, 760.f, DesignSize.x, 140.f, DeckStrip);
    decor.line({0.f, 760.f}, {DesignSize.x, 760.f}, 5.f, Ink);

    // The spyglass sweeping the sea from its tripod, with its light cone.
    const float sweep = std::sin(t * 1.2f) * 18.f - 12.f;
    decor.polyline({{210.f, 760.f}, {250.f, 600.f}, {290.f, 760.f}}, 12.f, DarkWood, true);
    auto rayEnd = [&](float degrees) {
        const float a = degrees * Pi / 180.f;
        return SpyglassCenter + sf::Vector2f(std::cos(a), std::sin(a)) * ConeLength;
    };
    decor.triangle(SpyglassCenter, rayEnd(sweep - 4.f), rayEnd(sweep + 4.f), Light);
    spyglass(decor, SpyglassCenter, 130.f, sweep);

    if (cards.empty())
    {
        waitingDots(front, {900.f, 450.f}, t, 16.f, 50.f);
        return;
    }

    const int chosen = std::clamp(selected, 0, static_cast<int>(cards.size()) - 1);
    const int first = chosen / CardsPerPage * CardsPerPage;
    const float hop = -std::abs(std::sin(t * Pi * HopFrequency)) * HopHeight;
    for (int slot = 0; slot < CardsPerPage; ++slot)
    {
        const int index = first + slot;
        if (index >= static_cast<int>(cards.size()))
            break;
        const DiscoveryCard& card = cards[static_cast<std::size_t>(index)];
        const bool on = index == chosen;
        const float b = on ? 1.f : DimmedBrightness;
        const float x = CardX + static_cast<float>(slot) * CardStep;
        const float cx = x + CardWidth / 2.f;

        roundedRect(decor, x, CardY, CardWidth, CardHeight, 20.f, on ? CardOn : CardOff);
        roundedFrame(front, x, CardY, CardWidth, CardHeight, 20.f, on ? 8.f : 4.f, on ? Gold : CardEdgeOff);

        // The host's pirate on its ship, crowned.
        ship(decor, {cx, 290.f}, 230.f, b);
        {
            const float size = 95.f;
            const float tileSize = size / 100.f * 27.f / 0.33f;
            CharacterRenderer pirate(tileSize, CharacterRenderer::Palette{}, card.hostModel);
            pirate.setBrightness(b);
            pirate.update(Character({0, 0}));
            pirate.setPosition(sf::Vector2f(cx, 215.f + (on ? hop * 0.5f : 0.f)) - sf::Vector2f(tileSize, tileSize) / 2.f);
            m_characters.push_back(std::move(pirate));
        }
        crown(front, {cx, 150.f}, 16.f, b);

        // Its map.
        auto miniature = m_miniatures.find(card.key);
        if (miniature == m_miniatures.end())
        {
            const float tile = std::min(MapSlotSize.x / static_cast<float>(std::max(card.map.getWidth(), 1)),
                                        MapSlotSize.y / static_cast<float>(std::max(card.map.getHeight(), 1)));
            GridRenderer renderer(tile, mapThemeFromName(card.map.getTheme()));
            renderer.update(card.map);
            miniature = m_miniatures.emplace(card.key, std::move(renderer)).first;
        }
        const sf::Vector2f mapSize(static_cast<float>(miniature->second.getPixelSize().x),
                                   static_cast<float>(miniature->second.getPixelSize().y));
        const sf::Vector2f mapPosition(cx - mapSize.x / 2.f, 420.f);
        miniature->second.setPosition(mapPosition);
        m_shown.push_back(card.key);
        if (!on)
            front.rect(mapPosition.x, mapPosition.y, mapSize.x, mapSize.y, MapDimming);

        // Its seats.
        const int seats = std::clamp(card.seatCount, 1, 4);
        for (int k = 0; k < seats; ++k)
        {
            const sf::Vector2f dot(cx + (static_cast<float>(k) - static_cast<float>(seats - 1) / 2.f) * 52.f, 660.f);
            if (k < card.seatsTaken)
            {
                front.circle(dot, 17.f, SeatColors[static_cast<std::size_t>(k)]);
                front.ring(dot, 17.f, 4.f, Ink);
            }
            else
            {
                front.circle(dot, 17.f, FreeSeat);
                dashedStroke(front, ellipsePoints(dot, 17.f, 17.f, 0.f, Mesh::segmentsFor(17.f)), 4.f, 6.f, 5.f,
                             on ? FreeSeatEdgeOn : FreeSeatEdgeOff);
            }
        }
    }
    waitingDots(front, {1400.f, 800.f}, t, 9.f, 30.f);

    // Forget the miniatures of hosts no longer heard.
    for (auto it = m_miniatures.begin(); it != m_miniatures.end();)
    {
        const bool heard = std::any_of(cards.begin(), cards.end(),
                                       [&](const DiscoveryCard& card) { return card.key == it->first; });
        it = heard ? std::next(it) : m_miniatures.erase(it);
    }
}

const sf::VertexArray& DiscoveryRenderer::getDecor() const noexcept
{
    return m_decor;
}

const sf::VertexArray& DiscoveryRenderer::getFront() const noexcept
{
    return m_front;
}

const std::vector<CharacterRenderer>& DiscoveryRenderer::getCharacters() const noexcept
{
    return m_characters;
}

std::vector<const GridRenderer*> DiscoveryRenderer::getShownMaps() const
{
    std::vector<const GridRenderer*> maps;
    for (const std::string& key : m_shown)
        if (const auto it = m_miniatures.find(key); it != m_miniatures.end())
            maps.push_back(&it->second);
    return maps;
}

int DiscoveryRenderer::getCardsShown() const noexcept
{
    return static_cast<int>(m_shown.size());
}

// =========================
// Private Methods
// =========================
void DiscoveryRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_decor, states);
    for (const CharacterRenderer& character : m_characters)
        target.draw(character, states);
    for (const GridRenderer* map : getShownMaps())
        target.draw(*map, states);
    target.draw(m_front, states);
}

} // namespace Bomberman

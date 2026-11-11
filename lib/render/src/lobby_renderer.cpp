#include "lobby_renderer.h"

#include <algorithm>
#include <cmath>

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
constexpr float SeatX = 40.f;
constexpr float SeatStep = 385.f;
constexpr float SeatY = 40.f;
constexpr float SeatWidth = 365.f;
constexpr float SeatHeight = 560.f;
constexpr float PirateY = 345.f;
constexpr sf::Vector2f MapPosition{90.f, 663.f};
constexpr sf::Vector2f WavesCenter{520.f, 770.f};
constexpr float LanternX = 740.f;
constexpr float LanternStep = 120.f;
constexpr sf::Vector2f LaunchCenter{1400.f, 748.f};

// Mock-up colors over their backgrounds (thin strokes opaque).
constexpr sf::Color EmptyPanelFill(40, 22, 10, 128);
constexpr sf::Color GhostEdge(163, 143, 122); // bone at 45 % over a ghost on an empty panel
constexpr sf::Color WaveUnlit(169, 127, 68); // gold at 35 % over the bottom panel
constexpr sf::Color LanternOff(58, 42, 28);
constexpr sf::Color LanternLight(255, 231, 168, 217);
constexpr float EmptyBrightness = 0.55f;
constexpr float HopHeight = 18.f;
constexpr float HopFrequency = 1.6f;
constexpr float PulseFrequency = 0.8f;

GridRenderer miniatureOf(const Grid& map)
{
    const float tile = std::min(LobbyRenderer::MapSlotSize.x / static_cast<float>(std::max(map.getWidth(), 1)),
                                LobbyRenderer::MapSlotSize.y / static_cast<float>(std::max(map.getHeight(), 1)));
    GridRenderer miniature(tile, mapThemeFromName(map.getTheme()));
    miniature.update(map);
    const sf::Vector2f size(static_cast<float>(miniature.getPixelSize().x),
                            static_cast<float>(miniature.getPixelSize().y));
    miniature.setPosition(MapPosition + (LobbyRenderer::MapSlotSize - size) / 2.f);
    return miniature;
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
LobbyRenderer::LobbyRenderer(const Grid& map)
    : m_decor(sf::PrimitiveType::Triangles),
      m_front(sf::PrimitiveType::Triangles),
      m_map(miniatureOf(map))
{
    update(LobbyView{});
}

// =========================
// Static Methods
// =========================
sf::View LobbyRenderer::viewFor(sf::Vector2u screenSize)
{
    return MatchSetupRenderer::viewFor(screenSize);
}

sf::FloatRect LobbyRenderer::seatBounds(int seat) noexcept
{
    return {{SeatX + static_cast<float>(seat) * SeatStep, SeatY}, {SeatWidth, SeatHeight}};
}

// =========================
// Public Methods
// =========================
void LobbyRenderer::setMap(const Grid& map)
{
    m_map = miniatureOf(map);
}

void LobbyRenderer::update(const LobbyView& view, float deltaSeconds)
{
    if (deltaSeconds > 0.f)
        m_elapsed = std::fmod(m_elapsed + deltaSeconds, 10.f); // every animation loops seamlessly in 10 s
    m_launchEnabled = view.isHost && view.canStart;
    m_decor.clear();
    m_front.clear();
    m_characters.clear();
    Mesh decor(m_decor);
    Mesh front(m_front);

    deckFloor(decor, DesignSize);
    const float hop = -std::abs(std::sin(m_elapsed * Pi * HopFrequency)) * HopHeight;

    for (int seat = 0; seat < LobbyView::SeatCount; ++seat)
    {
        const auto i = static_cast<std::size_t>(seat);
        const std::optional<CharacterModel>& model = view.seats[i];
        // A newcomer pops in; changing model does not.
        if (!model)
            m_seatedFor[i] = 0.f;
        else if (!m_previousSeats[i])
            m_seatedFor[i] = 0.f;
        else
            m_seatedFor[i] += deltaSeconds;
        m_previousSeats[i] = model;
        m_pirateCenters[i].reset();

        const sf::FloatRect bounds = seatBounds(seat);
        const float x = bounds.position.x;
        const float cx = x + SeatWidth / 2.f;
        const bool mine = seat == view.localSeat;
        const bool empty = !model.has_value();
        const sf::Color seatColor = SeatColors[i];

        roundedRect(decor, x, SeatY, SeatWidth, SeatHeight, 18.f,
                    empty ? EmptyPanelFill : mine ? PanelFocusedFill : PanelFill);
        roundedFrame(decor, x, SeatY, SeatWidth, SeatHeight, 18.f, mine ? 7.f : 4.f, mine ? Gold : PanelEdge);

        // Pennant of the seat's color with its number as dots.
        roundedRect(decor, x + 24.f, 62.f, SeatWidth - 48.f, 50.f, 12.f,
                    empty ? dimmed(seatColor, EmptyBrightness) : seatColor);
        roundedFrame(decor, x + 24.f, 62.f, SeatWidth - 48.f, 50.f, 12.f, 4.f, Ink);
        seatDots(decor, seat + 1, {cx, 87.f}, 8.f, 26.f);
        if (seat == 0)
            crown(decor, {cx, 150.f}, 26.f);

        if (empty)
        {
            ghost(decor, {cx, 340.f}, 190.f, GhostEdge);
            waitingDots(decor, {cx, 520.f}, m_elapsed + static_cast<float>(seat), 9.f, 30.f);
            continue;
        }

        decor.ellipse({cx, 455.f}, 112.f, 26.f, sf::Color(seatColor.r, seatColor.g, seatColor.b, 89));
        decor.stroke(ellipsePoints({cx, 455.f}, 112.f, 26.f, 0.f, Mesh::segmentsFor(112.f)), 6.f,
                     mine ? Gold : seatColor);
        const float pop = std::clamp(m_seatedFor[i] / PopDuration, 0.2f, 1.f);
        const sf::Vector2f center(cx, PirateY + (mine ? hop : 0.f));
        place(*model, center, (mine ? LocalPirateSize : SeatPirateSize) * pop, seat);
        m_pirateCenters[i] = center;
        if (mine)
        {
            arrow(decor, {x + 38.f, PirateY}, -1.f, 26.f);
            arrow(decor, {x + SeatWidth - 38.f, PirateY}, 1.f, 26.f);
        }
    }

    // Bottom band: the map, the network, the crew's lanterns, the button.
    roundedRect(decor, 40.f, 625.f, 1520.f, 245.f, 18.f, PanelFill);
    roundedFrame(decor, 40.f, 625.f, 1520.f, 245.f, 18.f, 4.f, PanelEdge);
    roundedFrame(front, MapPosition.x - 8.f, MapPosition.y - 8.f, MapSlotSize.x + 16.f, MapSlotSize.y + 16.f, 12.f,
                 4.f, FrameEdge);
    waves(decor, WavesCenter, 110.f, m_elapsed, Gold, WaveUnlit);
    for (int seat = 0; seat < LobbyView::SeatCount; ++seat)
    {
        const float lx = LanternX + static_cast<float>(seat) * LanternStep;
        const bool lit = view.seats[static_cast<std::size_t>(seat)].has_value();
        roundedRect(decor, lx - 30.f, 700.f, 60.f, 100.f, 14.f, lit ? SeatColors[static_cast<std::size_t>(seat)] : LanternOff);
        roundedFrame(decor, lx - 30.f, 700.f, 60.f, 100.f, 14.f, 5.f, Ink);
        if (lit)
            decor.circle({lx, 750.f}, 18.f, LanternLight);
        roundedRect(decor, lx - 14.f, 684.f, 28.f, 18.f, 5.f, BadgeFill);
    }
    if (view.isHost)
        launchButton(decor, LaunchCenter, 72.f, m_launchEnabled, m_elapsed, PulseFrequency);
    else
        hourglass(decor, LaunchCenter, 70.f, m_elapsed);
}

const sf::VertexArray& LobbyRenderer::getDecor() const noexcept
{
    return m_decor;
}

const sf::VertexArray& LobbyRenderer::getFront() const noexcept
{
    return m_front;
}

const std::vector<CharacterRenderer>& LobbyRenderer::getCharacters() const noexcept
{
    return m_characters;
}

const GridRenderer& LobbyRenderer::getMap() const noexcept
{
    return m_map;
}

bool LobbyRenderer::isLaunchEnabled() const noexcept
{
    return m_launchEnabled;
}

std::optional<sf::Vector2f> LobbyRenderer::getPirateCenter(int seat) const noexcept
{
    if (seat < 0 || seat >= LobbyView::SeatCount)
        return std::nullopt;
    return m_pirateCenters[static_cast<std::size_t>(seat)];
}

// =========================
// Private Methods
// =========================
void LobbyRenderer::place(CharacterModel model, sf::Vector2f center, float size, int seat)
{
    // Same placement as the launch menu: a body as big as the mock-up's
    // (radius 27 units of a size-wide view box) centered on center.
    const float tileSize = size / 100.f * 27.f / 0.33f;
    CharacterRenderer renderer(tileSize, CharacterRenderer::paletteForPlayer(seat), model);
    renderer.update(Character({0, 0}));
    renderer.setPosition(center - sf::Vector2f(tileSize / 2.f, tileSize / 2.f));
    m_characters.push_back(std::move(renderer));
}

void LobbyRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_decor, states);
    for (const CharacterRenderer& character : m_characters)
        target.draw(character, states);
    target.draw(m_map, states);
    target.draw(m_front, states);
}

} // namespace Bomberman

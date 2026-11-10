#include "match_setup_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "character.h"
#include "map_theme.h"
#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

// Mock-up colors. Thin strokes are opaque (see CLAUDE.md): the mock-up's
// translucent ink edges are replaced by the color they give over the deck.
constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color Gold(212, 169, 74);
constexpr sf::Color GoldGlow(212, 169, 74, 89);
constexpr sf::Color Deck(196, 144, 90);
constexpr sf::Color Seam(70, 40, 18, 115);
constexpr sf::Color Shadow(0, 0, 0, 64);
constexpr sf::Color PanelFill(40, 22, 10, 82);
constexpr sf::Color PanelFocusedFill(40, 22, 10, 140);
constexpr sf::Color PanelEdge(95, 70, 44); // ink at 60 % over the deck
constexpr sf::Color FrameEdge(63, 46, 28); // ink at 70 % over a panel
constexpr sf::Color BadgeFill(42, 26, 13);
constexpr sf::Color Bone(243, 234, 220);
constexpr sf::Color Blade(217, 221, 227);
constexpr sf::Color Grip(91, 57, 32);
constexpr sf::Color SlotOn(196, 144, 90);
constexpr sf::Color SlotOff(122, 86, 52);
constexpr sf::Color DotOff(255, 255, 255, 89);
constexpr sf::Color LaunchOff(109, 106, 99);
constexpr sf::Color BandanaRed(195, 59, 59);
constexpr sf::Color MapDimming(0, 0, 0, 102); // black at 40 %: the miniature at 60 % brightness
constexpr std::array<sf::Color, MatchSetup::DifficultyCount> DifficultyColors = {
    sf::Color(90, 166, 75), sf::Color(224, 138, 46), sf::Color(195, 59, 59)};

// Mock-up layout (design units).
constexpr std::array<float, CharacterModelCount> PirateX = {380.f, 615.f, 850.f, 1085.f, 1320.f};
constexpr float PirateY = 320.f;
constexpr float PirateSize = 150.f;
constexpr float SelectedPirateY = 315.f;
constexpr float SelectedPirateSize = 190.f;
constexpr int MapSlots = 4;
constexpr float MapX = 270.f;
constexpr float MapStep = 320.f;
constexpr float MapY = 500.f;
constexpr float ModeX = 520.f;
constexpr float ModeStep = 560.f;
constexpr float ModeY = 100.f;
constexpr float DifficultyX = 270.f;
constexpr float DifficultyStep = 210.f;
constexpr float PennantSize = 100.f;
constexpr float BottomY = 800.f;
constexpr float OpponentsX = 1000.f;
constexpr float OpponentsStep = 140.f;
constexpr sf::Vector2f LaunchCenter{1460.f, 800.f};
constexpr float LaunchRadius = 62.f;

sf::Color dimmed(sf::Color color, float brightness)
{
    auto scale = [brightness](std::uint8_t c) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(c) * brightness, 0.f, 255.f));
    };
    return sf::Color(scale(color.r), scale(color.g), scale(color.b), color.a);
}

void roundedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Color fill)
{
    mesh.polygon(roundedRectPoints(x, y, w, h, r, 6), fill);
}

void roundedFrame(Mesh& mesh, float x, float y, float w, float h, float r, float width, sf::Color color)
{
    mesh.stroke(roundedRectPoints(x, y, w, h, r, 6), width, color);
}

// The frame around a choice: gold and thick when it is the selected one.
void choiceFrame(Mesh& mesh, float x, float y, float w, float h, bool selected)
{
    roundedFrame(mesh, x, y, w, h, 12.f, selected ? 8.f : 4.f, selected ? Gold : FrameEdge);
}

std::vector<sf::Vector2f> transformed(std::vector<sf::Vector2f> points, sf::Vector2f pivot, float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    for (sf::Vector2f& p : points)
    {
        const sf::Vector2f d = p - pivot;
        p = pivot + sf::Vector2f(d.x * c - d.y * s, d.x * s + d.y * c);
    }
    return points;
}

// A filled rounded rectangle with an ink outline, turned by angle around
// pivot.
void turnedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Vector2f pivot, float angle,
                sf::Color fill, float outline)
{
    const std::vector<sf::Vector2f> points = transformed(roundedRectPoints(x, y, w, h, r, 4), pivot, angle);
    mesh.polygon(points, fill);
    mesh.stroke(points, outline, Ink);
}

// Two crossed cutlasses (the mode badge and the solo icon).
void swords(Mesh& mesh, sf::Vector2f c, float s, float brightness = 1.f)
{
    for (const float degrees : {35.f, -35.f})
    {
        const float angle = degrees * Pi / 180.f;
        turnedRect(mesh, c.x - s * 0.06f, c.y - s * 0.55f, s * 0.12f, s * 0.8f, s * 0.05f, c, angle,
                   dimmed(Blade, brightness), s * 0.04f);
        turnedRect(mesh, c.x - s * 0.2f, c.y + s * 0.2f, s * 0.4f, s * 0.09f, s * 0.04f, c, angle,
                   dimmed(Gold, brightness), s * 0.035f);
        turnedRect(mesh, c.x - s * 0.05f, c.y + s * 0.29f, s * 0.1f, s * 0.2f, s * 0.04f, c, angle,
                   dimmed(Grip, brightness), s * 0.035f);
    }
}

void appendCurve(std::vector<sf::Vector2f>& points, sf::Vector2f control, sf::Vector2f to, int segments = 8)
{
    const std::vector<sf::Vector2f> curve = quadraticBezier(points.back(), control, to, segments);
    points.insert(points.end(), curve.begin() + 1, curve.end());
}

// A skull of radius r centered on c: round cranium, jaw, two eyes, a nose.
void skull(Mesh& mesh, sf::Vector2f c, float r, float brightness = 1.f)
{
    std::vector<sf::Vector2f> outline = {{c.x - r, c.y}};
    appendCurve(outline, {c.x - r, c.y - r * 1.15f}, {c.x, c.y - r * 1.15f});
    appendCurve(outline, {c.x + r, c.y - r * 1.15f}, {c.x + r, c.y});
    appendCurve(outline, {c.x + r, c.y + r * 0.55f}, {c.x + r * 0.55f, c.y + r * 0.7f}, 5);
    outline.push_back({c.x + r * 0.55f, c.y + r * 1.05f});
    outline.push_back({c.x - r * 0.55f, c.y + r * 1.05f});
    outline.push_back({c.x - r * 0.55f, c.y + r * 0.7f});
    appendCurve(outline, {c.x - r, c.y + r * 0.55f}, {c.x - r, c.y}, 5);
    outline.pop_back(); // closed: the last point is the first one

    const sf::Color ink = dimmed(Ink, brightness);
    mesh.fillPolygon(outline, dimmed(Bone, brightness));
    mesh.stroke(outline, r * 0.14f, ink);
    mesh.ellipse({c.x - r * 0.4f, c.y}, r * 0.27f, r * 0.32f, ink);
    mesh.ellipse({c.x + r * 0.4f, c.y}, r * 0.27f, r * 0.32f, ink);
    mesh.triangle({c.x, c.y + r * 0.28f}, {c.x - r * 0.13f, c.y + r * 0.5f}, {c.x + r * 0.13f, c.y + r * 0.5f}, ink);
}

// One, two or three skulls on a green, orange or red pennant.
void pennant(Mesh& mesh, int difficulty, sf::Vector2f c, float s, float brightness)
{
    roundedRect(mesh, c.x - s * 0.95f, c.y - s * 0.55f, s * 1.9f, s * 1.1f, s * 0.2f,
                dimmed(DifficultyColors[static_cast<std::size_t>(difficulty)], brightness));
    roundedFrame(mesh, c.x - s * 0.95f, c.y - s * 0.55f, s * 1.9f, s * 1.1f, s * 0.2f, s * 0.07f,
                 dimmed(Ink, brightness));
    const int count = difficulty + 1;
    const float r = s * 0.26f;
    const float gap = r * 2.25f;
    for (int k = 0; k < count; ++k)
        skull(mesh, {c.x + (static_cast<float>(k) - static_cast<float>(count - 1) / 2.f) * gap, c.y + r * 0.1f}, r,
              brightness);
}

// The local multiplayer padlock.
void padlock(Mesh& mesh, sf::Vector2f c, float s)
{
    std::vector<sf::Vector2f> shackle = {{c.x - s * 0.22f, c.y}, {c.x - s * 0.22f, c.y - s * 0.18f}};
    appendCurve(shackle, {c.x - s * 0.22f, c.y - s * 0.45f}, {c.x, c.y - s * 0.45f});
    appendCurve(shackle, {c.x + s * 0.22f, c.y - s * 0.45f}, {c.x + s * 0.22f, c.y - s * 0.18f});
    shackle.push_back({c.x + s * 0.22f, c.y});
    mesh.polyline(shackle, s * 0.09f, Ink, true);
    roundedRect(mesh, c.x - s * 0.32f, c.y - s * 0.04f, s * 0.64f, s * 0.5f, s * 0.08f, Gold);
    roundedFrame(mesh, c.x - s * 0.32f, c.y - s * 0.04f, s * 0.64f, s * 0.5f, s * 0.08f, s * 0.06f, Ink);
    mesh.circle({c.x, c.y + s * 0.18f}, s * 0.07f, Ink);
}

// The buccaneer's bandana (pirate badge), mock-up group
// translate(c + (0, 8)) scale(.55) translate(-50 -50).
void bandanaBadge(Mesh& mesh, sf::Vector2f c)
{
    constexpr float k = 0.55f;
    auto at = [&](float x, float y) { return sf::Vector2f(c.x + (x - 50.f) * k, c.y + 8.f + (y - 50.f) * k); };
    std::vector<sf::Vector2f> points;
    for (int i = 0; i <= 32; ++i)
    {
        const float a = Pi + Pi * static_cast<float>(i) / 32.f;
        points.push_back(at(50.f + 28.f * std::cos(a), 50.f + 28.f * std::sin(a)));
    }
    constexpr std::array<std::array<float, 2>, 9> Trim = {
        {{72, 48}, {66, 54}, {60, 47}, {54, 53}, {50, 46}, {46, 53}, {40, 47}, {34, 54}, {28, 48}}};
    for (const auto& p : Trim)
        points.push_back(at(p[0], p[1]));
    mesh.fillPolygon(points, BandanaRed);
    mesh.stroke(points, 2.5f * k, Ink);
}

// Compass rose (map badge).
void compass(Mesh& mesh, sf::Vector2f c)
{
    const std::vector<sf::Vector2f> northSouth = {{c.x, c.y - 26.f}, {c.x + 8.f, c.y}, {c.x, c.y + 26.f}, {c.x - 8.f, c.y}};
    const std::vector<sf::Vector2f> eastWest = {{c.x - 26.f, c.y}, {c.x, c.y - 8.f}, {c.x + 26.f, c.y}, {c.x, c.y + 8.f}};
    mesh.polygon(northSouth, Gold);
    mesh.stroke(northSouth, 2.f, Ink);
    mesh.polygon(eastWest, Bone);
    mesh.stroke(eastWest, 2.f, Ink);
}

// Dark disc with a golden rim at the start of each row; its drawing (or
// the characters placed over it) says what the row sets.
void badge(Mesh& mesh, sf::Vector2f c)
{
    mesh.circle(c, 36.f, BadgeFill);
    mesh.ring(c, 36.f, 4.f, Gold);
}

void launchButton(Mesh& mesh, sf::Vector2f c, float r, bool enabled, float elapsed)
{
    const float scale = enabled ? 1.f + std::sin(elapsed * Pi * 2.f * MatchSetupRenderer::PulseFrequency) * 0.05f : 1.f;
    const float radius = r * scale;
    if (enabled)
        mesh.circle(c, radius * 1.3f, GoldGlow);
    mesh.circle(c, radius, enabled ? Gold : LaunchOff);
    mesh.ring(c, radius, radius * 0.1f, Ink);
    const std::vector<sf::Vector2f> play = {
        {c.x - radius * 0.3f, c.y - radius * 0.45f}, {c.x - radius * 0.3f, c.y + radius * 0.45f}, {c.x + radius * 0.5f, c.y}};
    mesh.polygon(play, Ink);
    mesh.stroke(play, radius * 0.08f, Ink);
}

int indexOf(CharacterModel model)
{
    const auto it = std::find(AllCharacterModels.begin(), AllCharacterModels.end(), model);
    return static_cast<int>(it - AllCharacterModels.begin());
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
MatchSetupRenderer::MatchSetupRenderer(const std::vector<Grid>& maps)
    : m_decor(sf::PrimitiveType::Triangles),
      m_overlay(sf::PrimitiveType::Triangles),
      m_front(sf::PrimitiveType::Triangles)
{
    for (const Grid& grid : maps)
    {
        const float tile = std::min(MapSlotSize.x / static_cast<float>(std::max(grid.getWidth(), 1)),
                                    MapSlotSize.y / static_cast<float>(std::max(grid.getHeight(), 1)));
        GridRenderer miniature(tile, mapThemeFromName(grid.getTheme()));
        miniature.update(grid);
        m_maps.push_back(std::move(miniature));
    }
    MatchSetup setup;
    setup.mapCount = static_cast<int>(m_maps.size());
    update(setup);
}

// =========================
// Static Methods
// =========================
sf::View MatchSetupRenderer::viewFor(sf::Vector2u screenSize)
{
    const sf::Vector2f screen(static_cast<float>(std::max(screenSize.x, 1u)),
                              static_cast<float>(std::max(screenSize.y, 1u)));
    const float scale = std::min(screen.x / DesignSize.x, screen.y / DesignSize.y);
    return sf::View(DesignSize / 2.f, screen / scale);
}

sf::FloatRect MatchSetupRenderer::rowBounds(MatchSetup::Row row) noexcept
{
    switch (row)
    {
    case MatchSetup::Row::Mode:
        return {{40.f, 30.f}, {1520.f, 140.f}};
    case MatchSetup::Row::Pirate:
        return {{40.f, 190.f}, {1520.f, 250.f}};
    case MatchSetup::Row::Map:
        return {{40.f, 460.f}, {1520.f, 250.f}};
    case MatchSetup::Row::Difficulty:
        return {{40.f, 730.f}, {760.f, 140.f}};
    case MatchSetup::Row::Opponents:
        break;
    }
    return {{820.f, 730.f}, {540.f, 140.f}};
}

// =========================
// Public Methods
// =========================
void MatchSetupRenderer::update(const MatchSetup& setup, float deltaSeconds)
{
    if (deltaSeconds > 0.f)
        m_elapsed = std::fmod(m_elapsed + deltaSeconds, 10.f); // 16 hops and 8 pulses: seamless wrap
    m_launchEnabled = setup.canStart() && setup.map < static_cast<int>(m_maps.size());
    m_characters.clear();
    m_decor.clear();
    m_overlay.clear();
    m_front.clear();
    Mesh decor(m_decor);
    Mesh overlay(m_overlay);
    Mesh front(m_front);

    // Deck planks.
    decor.rect(0.f, 0.f, DesignSize.x, DesignSize.y, Deck);
    for (float y = 60.f; y < DesignSize.y; y += 60.f)
        decor.rect(0.f, y - 2.f, DesignSize.x, 3.f, Seam);

    // One panel per row, the focused one framed in gold.
    for (int i = 0; i < MatchSetup::RowCount; ++i)
    {
        const auto row = static_cast<MatchSetup::Row>(i);
        const sf::FloatRect r = rowBounds(row);
        const bool focused = row == setup.focus;
        roundedRect(decor, r.position.x, r.position.y, r.size.x, r.size.y, 18.f, focused ? PanelFocusedFill : PanelFill);
        roundedFrame(decor, r.position.x, r.position.y, r.size.x, r.size.y, 18.f, focused ? 7.f : 4.f,
                     focused ? Gold : PanelEdge);
    }

    // Row badges.
    badge(decor, {95.f, ModeY});
    swords(decor, {95.f, ModeY}, 52.f);
    badge(decor, {95.f, 315.f});
    bandanaBadge(decor, {95.f, 315.f});
    badge(decor, {95.f, 585.f});
    compass(decor, {95.f, 585.f});
    badge(decor, {95.f, BottomY});
    skull(decor, {95.f, BottomY + 3.f}, 20.f);
    badge(decor, {875.f, BottomY});
    place(CharacterModel::Revenant, {866.f, BottomY + 2.f}, 38.f, 1.f);
    place(CharacterModel::Revenant, {886.f, BottomY + 4.f}, 38.f, 1.f);

    // Mode: the buccaneer against a skeleton, or two buccaneers under a
    // padlock (local multiplayer, not playable yet).
    for (int i = 0; i < GameModeCount; ++i)
    {
        const bool on = AllGameModes[static_cast<std::size_t>(i)] == setup.mode;
        const float b = on ? 1.f : DimmedBrightness;
        const float cx = ModeX + static_cast<float>(i) * ModeStep;
        choiceFrame(decor, cx - 200.f, 45.f, 400.f, 110.f, on);
        if (i == 0)
        {
            place(CharacterModel::Buccaneer, {cx - 55.f, ModeY}, 90.f, b);
            swords(overlay, {cx, ModeY - 5.f}, 50.f, b);
            place(CharacterModel::Revenant, {cx + 55.f, ModeY}, 90.f, b);
        }
        else
        {
            place(CharacterModel::Buccaneer, {cx - 45.f, ModeY}, 90.f, b);
            place(CharacterModel::Buccaneer, {cx + 45.f, ModeY}, 90.f, b, CharacterRenderer::paletteForPlayer(1));
            padlock(overlay, {cx, ModeY + 25.f}, 55.f);
        }
    }

    // Pirates: the others dimmed over their shadows, the chosen one in the
    // golden circle (drawn last).
    const int selected = indexOf(setup.pirate);
    for (int i = 0; i < CharacterModelCount; ++i)
    {
        if (i == selected)
            continue;
        const float x = PirateX[static_cast<std::size_t>(i)];
        decor.ellipse({x, 400.f}, 55.f, 12.f, Shadow);
        place(AllCharacterModels[static_cast<std::size_t>(i)], {x, PirateY}, PirateSize, DimmedBrightness);
    }
    const float selectedX = PirateX[static_cast<std::size_t>(selected)];
    overlay.ellipse({selectedX, 405.f}, 105.f, 24.f, GoldGlow);
    overlay.stroke(ellipsePoints({selectedX, 405.f}, 105.f, 24.f, 0.f, Mesh::segmentsFor(105.f)), 6.f, Gold);

    // Maps: miniatures of the real boards, four per page.
    const int firstMap = setup.map / MapSlots * MapSlots;
    m_firstMap = firstMap;
    for (int slot = 0; slot < MapSlots; ++slot)
    {
        const int i = firstMap + slot;
        if (i >= static_cast<int>(m_maps.size()))
            break;
        const bool on = i == setup.map;
        const float x = MapX + static_cast<float>(slot) * MapStep;
        GridRenderer& miniature = m_maps[static_cast<std::size_t>(i)];
        const sf::Vector2f size(static_cast<float>(miniature.getPixelSize().x),
                                static_cast<float>(miniature.getPixelSize().y));
        miniature.setPosition({x + (MapSlotSize.x - size.x) / 2.f, MapY + (MapSlotSize.y - size.y) / 2.f});
        if (!on)
            front.rect(x, MapY, MapSlotSize.x, MapSlotSize.y, MapDimming);
        choiceFrame(front, x - 8.f, MapY - 8.f, MapSlotSize.x + 16.f, MapSlotSize.y + 16.f, on);
    }

    // Difficulty pennants.
    for (int i = 0; i < MatchSetup::DifficultyCount; ++i)
    {
        const bool on = i == setup.difficulty;
        const float cx = DifficultyX + static_cast<float>(i) * DifficultyStep;
        pennant(decor, i, {cx, BottomY}, PennantSize, on ? 1.f : DimmedBrightness);
        if (on)
            choiceFrame(decor, cx - 105.f, 737.f, 210.f, 126.f, true);
    }

    // Opponents: each count in its own wooden slot, as many heads as
    // opponents and as many dots under them.
    for (int i = 0; i < MatchSetup::MaxOpponents; ++i)
    {
        const int count = i + 1;
        const bool on = count == setup.opponents;
        const float cx = OpponentsX + static_cast<float>(i) * OpponentsStep;
        roundedRect(decor, cx - 62.f, 745.f, 124.f, 110.f, 12.f, on ? SlotOn : SlotOff);
        roundedFrame(decor, cx - 62.f, 745.f, 124.f, 110.f, 12.f, 4.f, Ink);
        for (int k = 0; k < count; ++k)
        {
            const float offset = (static_cast<float>(k) - static_cast<float>(count - 1) / 2.f) * 58.f * 0.62f;
            place(CharacterModel::Revenant, {cx + offset, 795.f}, 58.f * 0.75f, on ? 1.f : DimmedBrightness);
        }
        for (int k = 0; k < count; ++k)
        {
            const sf::Vector2f dot(cx + (static_cast<float>(k) - static_cast<float>(i) / 2.f) * 18.f, 840.f);
            overlay.circle(dot, 5.f, on ? Gold : DotOff);
            overlay.ring(dot, 5.f, 2.f, Ink);
        }
        if (on)
            choiceFrame(overlay, cx - 62.f, 745.f, 124.f, 110.f, true);
    }

    launchButton(front, LaunchCenter, LaunchRadius, m_launchEnabled, m_elapsed);

    // The chosen pirate, hopping, last.
    const float hop = -std::abs(std::sin(m_elapsed * Pi * HopFrequency)) * HopHeight;
    m_selectedCenter = {selectedX, SelectedPirateY + hop};
    place(setup.pirate, m_selectedCenter, SelectedPirateSize, 1.f);
}

const sf::VertexArray& MatchSetupRenderer::getDecor() const noexcept
{
    return m_decor;
}

const sf::VertexArray& MatchSetupRenderer::getOverlay() const noexcept
{
    return m_overlay;
}

const sf::VertexArray& MatchSetupRenderer::getFront() const noexcept
{
    return m_front;
}

const std::vector<CharacterRenderer>& MatchSetupRenderer::getCharacters() const noexcept
{
    return m_characters;
}

sf::Vector2f MatchSetupRenderer::getSelectedPirateCenter() const noexcept
{
    return m_selectedCenter;
}

const std::vector<GridRenderer>& MatchSetupRenderer::getMaps() const noexcept
{
    return m_maps;
}

bool MatchSetupRenderer::isLaunchEnabled() const noexcept
{
    return m_launchEnabled;
}

// =========================
// Private Methods
// =========================
void MatchSetupRenderer::place(CharacterModel model, sf::Vector2f center, float size, float brightness,
                               CharacterRenderer::Palette palette)
{
    // A CharacterRenderer draws its character on cell (0, 0): pick the tile
    // size giving a body as big as the mock-up's (radius 27 units of a
    // size-wide view box) and move the body center onto center.
    const float tileSize = size / 100.f * 27.f / 0.33f;
    CharacterRenderer renderer(tileSize, palette, model);
    renderer.setBrightness(brightness);
    renderer.update(Character({0, 0}));
    renderer.setPosition(center - sf::Vector2f(tileSize / 2.f, tileSize / 2.f));
    m_characters.push_back(std::move(renderer));
}

void MatchSetupRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_decor, states);
    for (std::size_t i = 0; i + 1 < m_characters.size(); ++i)
        target.draw(m_characters[i], states);
    target.draw(m_overlay, states);
    if (!m_characters.empty())
        target.draw(m_characters.back(), states);

    // Only the current page of maps.
    const int firstMap = m_firstMap;
    for (int slot = 0; slot < MapSlots; ++slot)
    {
        const std::size_t i = static_cast<std::size_t>(firstMap + slot);
        if (i < m_maps.size())
            target.draw(m_maps[i], states);
    }
    target.draw(m_front, states);
}

} // namespace Bomberman

#include "game_over_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "character.h"
#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

constexpr sf::Color Ink(28, 20, 13);
constexpr sf::Color Gold(212, 169, 74);
constexpr sf::Color CoinGold(242, 201, 76);
constexpr sf::Color CoinLight(247, 220, 122);
constexpr sf::Color Iron(74, 74, 80);
constexpr sf::Color Rivet(165, 163, 157);
constexpr sf::Color Wood(138, 90, 46);
constexpr sf::Color DarkWood(91, 57, 32);
constexpr sf::Color Lining(58, 36, 20);
constexpr sf::Color FlagCloth(23, 23, 28);
constexpr sf::Color FlagBones(61, 60, 62); // bone white at 18 % over the cloth (opaque: thick strokes)
constexpr sf::Color Bone(236, 230, 214);
constexpr sf::Color SkullBone(243, 234, 220);
constexpr sf::Color VictoryLetters(242, 201, 76);
constexpr sf::Color Ruby(195, 59, 59);
constexpr sf::Color Emerald(63, 163, 77);
constexpr sf::Color Sapphire(47, 111, 174);
constexpr sf::Color Sparkle(255, 246, 216);

// Mock-up layout (design units).
constexpr sf::Vector2f ChestBase{800.f, 860.f}; // bottom center of the chest
constexpr float SoleRatio = (94.f - 56.f) / 100.f; // body center to soles, per unit of size

float clamp01(float u)
{
    return std::clamp(u, 0.f, 1.f);
}

float span(float t, float from, float to)
{
    return clamp01((t - from) / (to - from));
}

float lerp(float a, float b, float u)
{
    return a + (b - a) * u;
}

float easeOutBack(float u)
{
    constexpr float c = 1.7f;
    u = clamp01(u);
    return 1.f + (c + 1.f) * std::pow(u - 1.f, 3.f) + c * std::pow(u - 1.f, 2.f);
}

// The mock-up's deterministic hash(i, salt).
float hash(int i, int salt)
{
    const double x = std::sin(i * 127.1 + salt * 311.7) * 43758.5453;
    return static_cast<float>(x - std::floor(x));
}

sf::Color withAlpha(sf::Color color, float alpha)
{
    color.a = static_cast<std::uint8_t>(std::lround(255.f * clamp01(alpha)));
    return color;
}

// ---------- lettering: round strokes in a 70x100 box per letter ----------
using Stroke = std::vector<sf::Vector2f>;

void arc(Stroke& points, float cx, float cy, float rx, float ry, float fromDeg, float toDeg, int segments)
{
    for (int i = 0; i <= segments; ++i)
    {
        const float a = (fromDeg + (toDeg - fromDeg) * static_cast<float>(i) / static_cast<float>(segments)) * Pi / 180.f;
        points.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
}

std::vector<Stroke> glyph(char letter)
{
    switch (letter)
    {
    case 'G':
    {
        Stroke s;
        arc(s, 35.f, 50.f, 35.f, 50.f, -40.f, -330.f, 24);
        s.push_back({68.f, 55.f});
        s.push_back({42.f, 55.f});
        return {s};
    }
    case 'A':
        return {{{0.f, 100.f}, {35.f, 0.f}, {70.f, 100.f}}, {{14.f, 62.f}, {56.f, 62.f}}};
    case 'M':
        return {{{0.f, 100.f}, {0.f, 0.f}, {35.f, 58.f}, {70.f, 0.f}, {70.f, 100.f}}};
    case 'E':
        return {{{66.f, 0.f}, {0.f, 0.f}, {0.f, 100.f}, {66.f, 100.f}}, {{0.f, 50.f}, {52.f, 50.f}}};
    case 'O':
    {
        Stroke s;
        arc(s, 35.f, 50.f, 35.f, 50.f, 0.f, 360.f, 36);
        return {s};
    }
    case 'V':
        return {{{0.f, 0.f}, {35.f, 100.f}, {70.f, 0.f}}};
    case 'R':
    {
        Stroke bowl = {{0.f, 100.f}, {0.f, 0.f}, {38.f, 0.f}};
        arc(bowl, 38.f, 27.f, 29.f, 27.f, -90.f, 90.f, 12);
        bowl.push_back({0.f, 54.f});
        return {bowl, {{30.f, 54.f}, {70.f, 100.f}}};
    }
    default:
        return {};
    }
}

constexpr float LetterWidth = 70.f;
constexpr float LetterGap = 24.f;
constexpr float WordGap = 52.f;

float wordWidth(const std::string& text)
{
    float w = 0.f;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        w += text[i] == ' ' ? WordGap : LetterWidth;
        if (i + 1 < text.size())
            w += LetterGap;
    }
    return w;
}

// A stroke with round joins and caps; closed loops (first point = last)
// are stroked as such.
void roundStroke(Mesh& mesh, Stroke points, float width, sf::Color color)
{
    points.erase(std::unique(points.begin(), points.end(),
                             [](sf::Vector2f a, sf::Vector2f b) { return std::hypot(a.x - b.x, a.y - b.y) < 1e-3f; }),
                 points.end());
    if (points.size() < 2)
        return;
    const sf::Vector2f first = points.front();
    const sf::Vector2f last = points.back();
    if (std::hypot(first.x - last.x, first.y - last.y) < 1e-2f)
    {
        points.pop_back();
        mesh.stroke(points, width, color);
        return;
    }
    mesh.polyline(points, width, color, true);
    mesh.circle(first, width / 2.f, color);
    mesh.circle(last, width / 2.f, color);
}

// ---------- the open chest ("Le coffre débordant"), mock-up chestOpen ----------
void coin(Mesh& mesh, sf::Vector2f c, float r, float k)
{
    mesh.ellipse(c, r, r * 0.55f, CoinGold);
    mesh.stroke(ellipsePoints(c, r, r * 0.55f, 0.f, Mesh::segmentsFor(r)), 2.5f * k, Ink);
    mesh.ellipse({c.x - r * 0.1f, c.y - r * 0.08f}, r * 0.55f, r * 0.28f, CoinLight);
}

void coinStack(Mesh& mesh, float x, float bottom, int count, float r, float k)
{
    for (int i = 0; i < count; ++i)
        coin(mesh, {x + (i % 2 ? 2.f : -2.f) * k, bottom - static_cast<float>(i) * r * 0.38f}, r, k);
}

void gem(Mesh& mesh, sf::Vector2f c, float r, sf::Color color)
{
    const std::vector<sf::Vector2f> stone = {{c.x, c.y - r}, {c.x + r, c.y - r * 0.2f}, {c.x, c.y + r}, {c.x - r, c.y - r * 0.2f}};
    mesh.polygon(stone, color);
    mesh.stroke(stone, r * 0.18f, Ink);
    mesh.polygon({{c.x, c.y - r}, {c.x + r * 0.45f, c.y - r * 0.25f}, {c.x, c.y - r * 0.05f}, {c.x - r * 0.45f, c.y - r * 0.25f}},
                 sf::Color(255, 255, 255, 115));
}

void rivets(Mesh& mesh, float x, float from, float to, float k)
{
    for (const float u : {0.f, 0.5f, 1.f})
        mesh.circle({x, lerp(from, to, u)}, 4.f * k, Rivet);
}

void outlinedRect(Mesh& mesh, float x, float y, float w, float h, float r, sf::Color fill, float outline)
{
    const std::vector<sf::Vector2f> points = r > 0.f ? roundedRectPoints(x, y, w, h, r, 4)
                                                     : std::vector<sf::Vector2f>{{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    mesh.polygon(points, fill);
    mesh.stroke(points, outline, Ink);
}

void sparkle(Mesh& mesh, sf::Vector2f c, float r)
{
    if (r <= 0.5f)
        return;
    mesh.fan(c,
             {{c.x, c.y - r}, {c.x + r * 0.25f, c.y - r * 0.25f}, {c.x + r, c.y}, {c.x + r * 0.25f, c.y + r * 0.25f},
              {c.x, c.y + r}, {c.x - r * 0.25f, c.y + r * 0.25f}, {c.x - r, c.y}, {c.x - r * 0.25f, c.y - r * 0.25f}},
             Sparkle);
}

std::vector<sf::Vector2f> lidPoints(float x0, float w, float top, float cx, float k, float inset)
{
    const float left = x0 + (6.f + inset) * k;
    const float right = x0 + w - (6.f + inset) * k;
    const float shoulder = top - (120.f - inset * 0.4f) * k;
    const float crest = top - (170.f - inset) * k;
    std::vector<sf::Vector2f> points = {{left, top - 4.f * k}};
    for (const sf::Vector2f p : quadraticBezier({left, shoulder}, {left, crest}, {cx, crest}, 10))
        points.push_back(p);
    const std::vector<sf::Vector2f> second = quadraticBezier({cx, crest}, {right, crest}, {right, shoulder}, 10);
    points.insert(points.end(), second.begin() + 1, second.end());
    points.push_back({right, top - 4.f * k});
    return points;
}

// What goes behind the winner (the lid and the heap of gold).
void chestBack(Mesh& mesh, float cx, float bottom, float k)
{
    const float w = 300.f * k;
    const float h = 130.f * k;
    const float x0 = cx - w / 2.f;
    const float top = bottom - h;
    mesh.ellipse({cx, bottom + 6.f * k}, 185.f * k, 22.f * k, sf::Color(0, 0, 0, 89));

    const std::vector<sf::Vector2f> lid = lidPoints(x0, w, top, cx, k, 0.f);
    mesh.polygon(lid, DarkWood);
    mesh.stroke(lid, 6.f * k, Ink);
    mesh.polygon(lidPoints(x0, w, top, cx, k, 18.f), Lining);
    for (const float bx : {x0 + 34.f * k, x0 + w - 56.f * k})
    {
        outlinedRect(mesh, bx, top - 128.f * k, 22.f * k, 124.f * k, 0.f, Iron, 3.f * k);
        rivets(mesh, bx + 11.f * k, top - 112.f * k, top - 20.f * k, k);
    }

    for (int row = 3; row >= 0; --row)
    {
        const int n = 9 - 2 * row;
        const float spread = (250.f - 55.f * static_cast<float>(row)) * k;
        const float y = top - 6.f * k - static_cast<float>(row) * 17.f * k;
        for (int i = 0; i < n; ++i)
        {
            const float x = cx - spread / 2.f
                          + (n == 1 ? spread / 2.f : static_cast<float>(i) * spread / static_cast<float>(n - 1))
                          + (row % 2 ? 6.f : -6.f) * k;
            coin(mesh, {x, y + (i % 2 ? 4.f : 0.f) * k}, 22.f * k, k);
        }
    }
    gem(mesh, {cx - 70.f * k, top - 34.f * k}, 13.f * k, Ruby);
    gem(mesh, {cx + 78.f * k, top - 28.f * k}, 12.f * k, Emerald);
    gem(mesh, {cx + 28.f * k, top - 64.f * k}, 11.f * k, Sapphire);
}

// What goes in front of the winner's feet: the rim coins, the chest's
// front, the coins that rolled away and the sparkles.
void chestFront(Mesh& mesh, float cx, float bottom, float k, float t)
{
    const float w = 300.f * k;
    const float h = 130.f * k;
    const float x0 = cx - w / 2.f;
    const float top = bottom - h;
    for (int i = 0; i < 8; ++i)
        coin(mesh, {x0 + 22.f * k + static_cast<float>(i) * 37.f * k, top - 10.f * k + (i % 2 ? 3.f : 0.f) * k}, 21.f * k, k);

    outlinedRect(mesh, x0, top, w, h, 12.f * k, Wood, 6.f * k);
    mesh.rect(x0 + 3.f * k, top + 44.f * k, w - 6.f * k, 4.f * k, DarkWood);
    mesh.rect(x0 + 3.f * k, top + 88.f * k, w - 6.f * k, 4.f * k, DarkWood);
    outlinedRect(mesh, x0 - 5.f * k, top - 6.f * k, w + 10.f * k, 20.f * k, 6.f * k, Iron, 4.f * k);
    for (const float bx : {x0 + 26.f * k, x0 + w - 48.f * k})
    {
        outlinedRect(mesh, bx, top + 12.f * k, 22.f * k, h - 14.f * k, 0.f, Iron, 3.f * k);
        rivets(mesh, bx + 11.f * k, top + 28.f * k, bottom - 16.f * k, k);
    }
    outlinedRect(mesh, cx - 22.f * k, top + 10.f * k, 44.f * k, 56.f * k, 8.f * k, Gold, 4.f * k);
    mesh.circle({cx, top + 32.f * k}, 7.f * k, Ink);
    mesh.triangle({cx - 5.f * k, top + 34.f * k}, {cx + 5.f * k, top + 34.f * k}, {cx, top + 52.f * k}, Ink);

    coin(mesh, {x0 - 26.f * k, bottom - 4.f * k}, 20.f * k, k);
    coinStack(mesh, x0 + w + 30.f * k, bottom - 2.f * k, 3, 20.f * k, k);
    coin(mesh, {x0 + w - 4.f * k, bottom + 2.f * k}, 18.f * k, k);

    const std::array<std::array<float, 3>, 4> sparkles = {{{cx - 100.f * k, top - 40.f * k, 14.f * k},
                                                           {cx + 40.f * k, top - 82.f * k, 12.f * k},
                                                           {cx + 120.f * k, top - 20.f * k, 11.f * k},
                                                           {x0 - 20.f * k, bottom - 30.f * k, 10.f * k}}};
    for (std::size_t i = 0; i < sparkles.size(); ++i)
        sparkle(mesh, {sparkles[i][0], sparkles[i][1]},
                sparkles[i][2] * std::max(0.f, std::sin(t * 3.2f + static_cast<float>(i) * 1.7f)));
}

// Appends what draw builds, scaled by scale around pivot.
template <typename Draw>
void scaledAround(sf::VertexArray& vertices, sf::Vector2f pivot, float scale, Draw draw)
{
    const std::size_t first = vertices.getVertexCount();
    Mesh mesh(vertices);
    draw(mesh);
    for (std::size_t i = first; i < vertices.getVertexCount(); ++i)
        vertices[i].position = pivot + (vertices[i].position - pivot) * scale;
}

void skull(Mesh& mesh, sf::Vector2f c, float r)
{
    std::vector<sf::Vector2f> outline = {{c.x - r, c.y}};
    auto curve = [&](sf::Vector2f control, sf::Vector2f to, int segments) {
        const std::vector<sf::Vector2f> points = quadraticBezier(outline.back(), control, to, segments);
        outline.insert(outline.end(), points.begin() + 1, points.end());
    };
    curve({c.x - r, c.y - r * 1.15f}, {c.x, c.y - r * 1.15f}, 10);
    curve({c.x + r, c.y - r * 1.15f}, {c.x + r, c.y}, 10);
    curve({c.x + r, c.y + r * 0.55f}, {c.x + r * 0.55f, c.y + r * 0.7f}, 6);
    outline.push_back({c.x + r * 0.55f, c.y + r * 1.05f});
    outline.push_back({c.x - r * 0.55f, c.y + r * 1.05f});
    outline.push_back({c.x - r * 0.55f, c.y + r * 0.7f});
    curve({c.x - r, c.y + r * 0.55f}, {c.x - r, c.y}, 6);
    outline.pop_back();
    mesh.fillPolygon(outline, SkullBone);
    mesh.stroke(outline, r * 0.12f, Ink);
    mesh.ellipse({c.x - r * 0.4f, c.y}, r * 0.27f, r * 0.32f, Ink);
    mesh.ellipse({c.x + r * 0.4f, c.y}, r * 0.27f, r * 0.32f, Ink);
    mesh.triangle({c.x, c.y + r * 0.28f}, {c.x - r * 0.13f, c.y + r * 0.5f}, {c.x + r * 0.13f, c.y + r * 0.5f}, Ink);
}

// Golden crown, bottom center (cx, y), w wide.
void crown(Mesh& mesh, float cx, float y, float w)
{
    const float h = w * 0.55f;
    const std::array<std::array<float, 2>, 7> shape = {
        {{0.f, h}, {0.f, h * 0.25f}, {0.25f, h * 0.6f}, {0.5f, 0.f}, {0.75f, h * 0.6f}, {1.f, h * 0.25f}, {1.f, h}}};
    std::vector<sf::Vector2f> points;
    for (const auto& p : shape)
        points.push_back({cx - w / 2.f + p[0] * w, y - h + p[1]});
    mesh.fillPolygon(points, Gold);
    mesh.stroke(points, w * 0.05f, Ink);
    for (const float u : {0.f, 0.5f, 1.f})
    {
        const sf::Vector2f jewel(cx - w / 2.f + u * w, y - h + (u == 0.5f ? 0.f : h * 0.25f));
        mesh.circle(jewel, w * 0.07f, Ruby);
        mesh.ring(jewel, w * 0.07f, w * 0.03f, Ink);
    }
}

} // namespace

// =========================
// Static Methods
// =========================
sf::View GameOverRenderer::viewFor(sf::Vector2u screenSize)
{
    const sf::Vector2f screen(static_cast<float>(std::max(screenSize.x, 1u)),
                              static_cast<float>(std::max(screenSize.y, 1u)));
    const float scale = std::min(screen.x / DesignSize.x, screen.y / DesignSize.y);
    return sf::View(DesignSize / 2.f, screen / scale);
}

// =========================
// Public Methods
// =========================
void GameOverRenderer::start(MatchOutcome outcome, std::optional<CharacterModel> winner,
                             CharacterRenderer::Palette winnerPalette)
{
    m_started = true;
    m_elapsed = 0.f;
    m_outcome = outcome;
    m_winnerModel = outcome == MatchOutcome::Draw ? std::nullopt : winner;
    m_winnerPalette = winnerPalette;
    rebuild();
}

void GameOverRenderer::update(float deltaSeconds)
{
    if (!m_started)
        return;
    if (deltaSeconds > 0.f)
        m_elapsed = std::min(m_elapsed + deltaSeconds, Duration);
    rebuild();
}

bool GameOverRenderer::isStarted() const noexcept
{
    return m_started;
}

bool GameOverRenderer::isFinished() const noexcept
{
    return m_started && m_elapsed >= Duration;
}

float GameOverRenderer::getElapsed() const noexcept
{
    return m_elapsed;
}

MatchOutcome GameOverRenderer::getOutcome() const noexcept
{
    return m_outcome;
}

const sf::VertexArray& GameOverRenderer::getBack() const noexcept
{
    return m_back;
}

const sf::VertexArray& GameOverRenderer::getFront() const noexcept
{
    return m_front;
}

const std::optional<CharacterRenderer>& GameOverRenderer::getWinner() const noexcept
{
    return m_winner;
}

sf::Vector2f GameOverRenderer::getLetterCenter(int i) const noexcept
{
    return m_letters[std::clamp(i, 0, LetterCount - 1)];
}

// =========================
// Private Methods
// =========================
void GameOverRenderer::rebuild()
{
    const float t = m_elapsed;
    m_back.clear();
    m_front.clear();
    m_winner.reset();
    Mesh back(m_back);
    Mesh front(m_front);

    // The frozen board darkens (beyond the design space: the bars too).
    back.rect(-4000.f, -4000.f, DesignSize.x + 8000.f, DesignSize.y + 8000.f,
              sf::Color(8, 6, 4, static_cast<std::uint8_t>(std::lround(255.f * 0.7f * span(t, 0.f, 0.4f)))));

    // The flag drops from the top and waves.
    const float flagY = lerp(-330.f, 70.f, easeOutBack(span(t, 0.05f, 0.5f)));
    outlinedRect(back, 200.f, flagY - 40.f, 22.f, 380.f, 8.f, DarkWood, 5.f);
    back.circle({211.f, flagY - 44.f}, 16.f, Gold);
    back.ring({211.f, flagY - 44.f}, 16.f, 5.f, Ink);
    std::vector<sf::Vector2f> top;
    std::vector<sf::Vector2f> bottom;
    for (int i = 0; i <= 24; ++i)
    {
        const float u = static_cast<float>(i) / 24.f;
        const float x = 230.f + u * 1140.f;
        const float wave = std::sin(u * Pi * 3.f - t * 5.f) * 14.f * u;
        top.push_back({x, flagY + wave});
        bottom.push_back({x, flagY + 300.f + wave});
    }
    for (std::size_t i = 0; i + 1 < top.size(); ++i)
        back.quad(top[i], top[i + 1], bottom[i + 1], bottom[i], FlagCloth);
    std::vector<sf::Vector2f> cloth = top;
    cloth.insert(cloth.end(), bottom.rbegin(), bottom.rend());
    back.stroke(cloth, 6.f, Ink);
    back.line({640.f, flagY + 40.f}, {960.f, flagY + 260.f}, 34.f, FlagBones, true);
    back.line({960.f, flagY + 40.f}, {640.f, flagY + 260.f}, 34.f, FlagBones, true);

    // GAME OVER, letter by letter.
    const std::string text = "GAME OVER";
    const float height = 120.f;
    const float k = height / 100.f;
    const sf::Color fill = m_outcome == MatchOutcome::Victory ? VictoryLetters : Bone;
    float x = 800.f - wordWidth(text) * k / 2.f;
    int letter = 0;
    for (const char c : text)
    {
        if (c == ' ')
        {
            x += (WordGap + LetterGap) * k;
            continue;
        }
        const float start = 0.35f + static_cast<float>(letter) * 0.06f;
        const float u = span(t, start, start + 0.35f);
        const sf::Vector2f center(x + LetterWidth * k / 2.f, flagY + 150.f + (1.f - easeOutBack(u)) * -260.f);
        m_letters[static_cast<std::size_t>(letter)] = center;
        if (u > 0.f)
        {
            const auto place = [&](sf::Vector2f p) { return center + (p - sf::Vector2f(35.f, 50.f)) * k; };
            for (const float width : {30.f, 16.f})
                for (Stroke stroke : glyph(c))
                {
                    std::transform(stroke.begin(), stroke.end(), stroke.begin(), place);
                    roundStroke(back, stroke, width * k, width > 20.f ? Ink : fill);
                }
        }
        x += (LetterWidth + LetterGap) * k;
        ++letter;
    }

    // The winner on the treasure, under a spotlight.
    const float appear = span(t, 0.55f, 0.95f);
    const float chestPop = easeOutBack(clamp01(appear * 2.f));
    const float feet = ChestBase.y - 130.f - 46.f;
    if (appear > 0.f)
    {
        back.polygon({{740.f, 370.f}, {860.f, 370.f}, {1060.f, 900.f}, {540.f, 900.f}},
                     withAlpha(sf::Color(255, 236, 180), 0.16f * appear));
        scaledAround(m_back, ChestBase, chestPop, [&](Mesh& mesh) { chestBack(mesh, ChestBase.x, ChestBase.y, 1.f); });
    }

    const float winnerPop = easeOutBack(appear);
    const sf::Vector2f winnerCenter(ChestBase.x, feet - WinnerSize * SoleRatio);
    if (appear > 0.f && winnerPop > 0.f)
    {
        const float hop = -std::abs(std::sin(t * Pi * 1.6f)) * 18.f;
        if (m_winnerModel)
        {
            const float size = WinnerSize * winnerPop;
            const float tileSize = size / 100.f * 27.f / 0.33f;
            CharacterRenderer winner(tileSize, m_winnerPalette, *m_winnerModel);
            winner.update(Character({0, 0}));
            const sf::Vector2f body(winnerCenter.x, winnerCenter.y + hop * winnerPop);
            winner.setPosition(body - sf::Vector2f(tileSize / 2.f, tileSize / 2.f));
            m_winner = std::move(winner);
            crown(front, body.x, body.y - size * 0.3f, size * 0.42f);
        }
        else
            scaledAround(m_front, winnerCenter, winnerPop,
                         [&](Mesh& mesh) { skull(mesh, winnerCenter, WinnerSize * 0.32f); });
    }
    if (appear > 0.f)
        scaledAround(m_front, ChestBase, chestPop,
                     [&](Mesh& mesh) { chestFront(mesh, ChestBase.x, ChestBase.y, 1.f, t); });

    // Gold raining down on a victory.
    if (m_outcome == MatchOutcome::Victory)
        for (int i = 0; i < 22; ++i)
        {
            const float u = t - 0.6f - hash(i, 4) * 0.8f;
            if (u < 0.f)
                continue;
            const float y = -40.f + u * (420.f + hash(i, 6) * 260.f);
            if (y > 940.f)
                continue;
            const sf::Vector2f c(120.f + hash(i, 5) * 1360.f, y);
            const float r = 14.f + hash(i, 7) * 8.f;
            const float rx = std::max(2.f, r * std::abs(std::cos(u * 6.f + static_cast<float>(i))));
            front.ellipse(c, rx, r, Gold);
            front.stroke(ellipsePoints(c, rx, r, 0.f, Mesh::segmentsFor(r)), 3.f, Ink);
        }
}

void GameOverRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    if (!m_started)
        return;
    target.draw(m_back, states);
    if (m_winner)
        target.draw(*m_winner, states);
    target.draw(m_front, states);
}

} // namespace Bomberman

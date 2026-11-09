#include "grid_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mesh.h"

namespace Bomberman
{

namespace
{

constexpr float Pi = 3.14159265f;

// Deterministic per-cell noise, so the board looks exactly the same after
// every update() (destroying a crate must not reshuffle the floor planks).
float noise(int x, int y, int salt)
{
    std::uint32_t h = (static_cast<std::uint32_t>(x + 101) * 374761393u)
                    ^ (static_cast<std::uint32_t>(y + 211) * 668265263u)
                    ^ (static_cast<std::uint32_t>(salt + 17) * 1442695041u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h) / 4294967295.f;
}

sf::Color rgb(std::uint32_t hex, std::uint8_t alpha = 255)
{
    return sf::Color(static_cast<std::uint8_t>((hex >> 16) & 0xFF), static_cast<std::uint8_t>((hex >> 8) & 0xFF),
                     static_cast<std::uint8_t>(hex & 0xFF), alpha);
}

// Mixes towards white (t > 0) or black (t < 0).
sf::Color shade(sf::Color color, float t)
{
    const float target = t > 0.f ? 255.f : 0.f;
    const float k = std::min(std::abs(t), 1.f);
    const auto mix = [&](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(v) + (target - static_cast<float>(v)) * k));
    };
    return sf::Color(mix(color.r), mix(color.g), mix(color.b), color.a);
}

float thickness(float tileSize, float fraction)
{
    return std::max(1.f, tileSize * fraction);
}

// Irregular boulder outline, star-shaped around its center.
std::vector<sf::Vector2f> rockPoints(sf::Vector2f center, float r, int gx, int gy)
{
    std::vector<sf::Vector2f> points;
    for (int i = 0; i < 8; ++i)
    {
        const float a = static_cast<float>(i) / 8.f * 2.f * Pi + 0.3f;
        const float radius = r * (0.82f + 0.2f * noise(gx, gy, 120 + i));
        points.push_back({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius * 0.9f});
    }
    return points;
}

struct Cell
{
    float x;
    float y;
    float s; // tile size
    int gx;
    int gy;

    [[nodiscard]] sf::Vector2f center() const { return {x + s / 2.f, y + s / 2.f}; }
    [[nodiscard]] sf::Vector2f at(float u, float v) const { return {x + s * u, y + s * v}; }
};

// =========================
// Floors
// =========================
void floorDeck(Mesh& m, const Cell& c)
{
    const float h = c.s / 2.f;
    const float seam = thickness(c.s, 0.025f);
    for (int b = 0; b < 2; ++b)
    {
        const float by = c.y + static_cast<float>(b) * h;
        m.rect(c.x, by, c.s, h, shade(rgb(0xc18d52), (noise(c.gx, c.gy * 2 + b, 1) - 0.5f) * 0.14f));
        for (int g = 0; g < 2; ++g)
        {
            const float yy = by + h * (0.32f + 0.36f * static_cast<float>(g))
                           + (noise(c.gx, c.gy, b * 7 + g) - 0.5f) * h * 0.18f;
            m.line({c.x + c.s * 0.04f, yy}, {c.x + c.s * 0.96f, yy + (noise(c.gx, c.gy, g + 11) - 0.5f) * c.s * 0.05f},
                   thickness(c.s, 0.014f), rgb(0x5f3a1a, 51));
        }
        m.rect(c.x, by + h - seam, c.s, seam, rgb(0x462812, 140));
        if (noise(c.gx, c.gy, b + 20) < 0.35f)
        {
            const float jx = c.x + c.s * (0.25f + 0.5f * noise(c.gx, c.gy, b + 30));
            m.rect(jx, by, seam, h, rgb(0x462812, 140));
            m.circle({jx - c.s * 0.06f, by + h * 0.5f}, c.s * 0.022f, rgb(0x4a2c14), 8);
            m.circle({jx + c.s * 0.08f, by + h * 0.5f}, c.s * 0.022f, rgb(0x4a2c14), 8);
        }
    }
}

void floorSand(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, shade(rgb(0xe4c48a), (noise(c.gx, c.gy, 2) - 0.5f) * 0.07f));
    for (int i = 0; i < 8; ++i)
    {
        const sf::Color color = (i % 2) ? rgb(0x966c3a, 89) : rgb(0xfff6de, 140);
        m.circle(c.at(noise(c.gx, c.gy, 40 + i), noise(c.gx, c.gy, 60 + i)),
                 c.s * (0.012f + noise(c.gx, c.gy, 80 + i) * 0.018f), color, 6);
    }
    if (noise(c.gx, c.gy, 9) < 0.25f)
        m.arc(c.at(0.5f, 1.1f), c.s * 0.55f, Pi * 1.2f, Pi * 1.8f, thickness(c.s, 0.02f), rgb(0xaa8046, 89));
}

void floorStone(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, rgb(0x26252b));
    m.polygon(roundedRectPoints(c.x + c.s * 0.05f, c.y + c.s * 0.05f, c.s * 0.9f, c.s * 0.9f, c.s * 0.12f),
              shade(rgb(0x55545c), (noise(c.gx, c.gy, 3) - 0.5f) * 0.18f));
    m.line(c.at(0.14f, 0.09f), c.at(0.86f, 0.09f), thickness(c.s, 0.02f), rgb(0xffffff, 18));
    if (noise(c.gx, c.gy, 5) < 0.3f)
        m.polyline({c.at(0.2f, 0.3f + noise(c.gx, c.gy, 6) * 0.3f), c.at(0.45f, 0.5f), c.at(0.62f, 0.78f)},
                   thickness(c.s, 0.02f), rgb(0x121216, 153));
}

void floorDock(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, rgb(0x1d3f52)); // water between the planks
    const float w = c.s / 3.f;
    const float gap = thickness(c.s, 0.035f);
    for (int i = 0; i < 3; ++i)
    {
        const float px = c.x + static_cast<float>(i) * w;
        m.rect(px + gap / 2.f, c.y, w - gap, c.s, shade(rgb(0x9a917d), (noise(c.gx * 3 + i, c.gy, 4) - 0.5f) * 0.16f));
        m.line({px + w * 0.4f, c.y + c.s * 0.05f}, {px + w * 0.45f, c.y + c.s * 0.95f}, thickness(c.s, 0.012f),
               rgb(0x3c372d, 64));
        m.circle({px + w / 2.f, c.y + c.s * 0.12f}, c.s * 0.02f, rgb(0x3d3830), 6);
        m.circle({px + w / 2.f, c.y + c.s * 0.88f}, c.s * 0.02f, rgb(0x3d3830), 6);
    }
}

// =========================
// Border walls
// =========================
void wallBulwark(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, rgb(0x5b3920));
    for (int i = 1; i < 3; ++i)
        m.rect(c.x + static_cast<float>(i) * c.s / 3.f, c.y, thickness(c.s, 0.03f), c.s, rgb(0x000000, 71));
    m.rect(c.x, c.y, c.s, c.s * 0.22f, rgb(0x7a4f2c));
    m.rect(c.x, c.y, c.s, c.s * 0.05f, rgb(0xffdca0, 41));
    m.circle(c.at(0.25f, 0.62f), c.s * 0.035f, rgb(0xd4a94a), 10);
    m.circle(c.at(0.75f, 0.62f), c.s * 0.035f, rgb(0xd4a94a), 10);
}

void wallSea(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, shade(rgb(0x2b7aa3), (noise(c.gx, c.gy, 7) - 0.5f) * 0.08f));
    for (int i = 0; i < 2; ++i)
        m.arc(c.at(0.2f + 0.6f * noise(c.gx, c.gy, 90 + i), 0.25f + 0.5f * noise(c.gx, c.gy, 95 + i)), c.s * 0.12f,
              Pi * 1.15f, Pi * 1.85f, thickness(c.s, 0.035f), rgb(0xffffff, 82));
}

void wallCave(Mesh& m, const Cell& c, bool border)
{
    m.rect(c.x, c.y, c.s, c.s, border ? rgb(0x1c1b20) : rgb(0x232127));
    const auto clampToCell = [&](sf::Vector2f p) {
        return sf::Vector2f{std::clamp(p.x, c.x, c.x + c.s), std::clamp(p.y, c.y, c.y + c.s)};
    };
    for (int i = 0; i < 3; ++i)
    {
        const sf::Vector2f p = c.at(noise(c.gx, c.gy, 150 + i), noise(c.gx, c.gy, 160 + i));
        const float r = c.s * (0.18f + 0.14f * noise(c.gx, c.gy, 170 + i));
        m.quad(clampToCell({p.x, p.y - r}), clampToCell({p.x + r, p.y - r * 0.2f}),
               clampToCell({p.x + r * 0.5f, p.y + r}), clampToCell({p.x - r * 0.8f, p.y + r * 0.4f}),
               i == 0 ? rgb(0x3a3840) : rgb(0x2f2d35));
    }
    m.line(c.at(0.1f, 0.2f), c.at(0.5f, 0.12f), thickness(c.s, 0.02f), rgb(0xffffff, 13));
}

void wallWater(Mesh& m, const Cell& c)
{
    m.rect(c.x, c.y, c.s, c.s, shade(rgb(0x1f5571), (noise(c.gx, c.gy, 8) - 0.5f) * 0.08f));
    for (int i = 0; i < 2; ++i)
    {
        const float v = 0.3f + 0.4f * static_cast<float>(i) + (noise(c.gx, c.gy, 200 + i) - 0.5f) * 0.1f;
        std::vector<sf::Vector2f> wave;
        for (int k = 0; k <= 8; ++k)
        {
            const float t = static_cast<float>(k) / 8.f;
            wave.push_back(c.at(0.15f + 0.7f * t, v - std::sin(t * 2.f * Pi) * 0.03f));
        }
        m.polyline(wave, thickness(c.s, 0.03f), rgb(0xb4dcf0, 56));
    }
}

// =========================
// Pillars (inner walls), drawn on top of the floor
// =========================
void pillarMast(Mesh& m, const Cell& c)
{
    const sf::Vector2f center = c.center();
    const float r = c.s * 0.4f;
    const float outline = thickness(c.s, 0.05f);
    m.circle(center + sf::Vector2f{c.s * 0.05f, c.s * 0.07f}, r, rgb(0x000000, 77), 28);
    m.circle(center, r + outline / 2.f, rgb(0x2a180c), 28);
    m.circle(center, r - outline / 2.f, rgb(0x6b4426), 28);
    m.ring(center, c.s * 0.3f, thickness(c.s, 0.07f), rgb(0x3a3a40));
    m.circle(center, c.s * 0.2f, rgb(0x7d5230), 20);
    m.arc(center, c.s * 0.34f, Pi * 1.1f, Pi * 1.45f, thickness(c.s, 0.03f), rgb(0xffe6b4, 64), 6);
}

void pillarRock(Mesh& m, const Cell& c)
{
    const sf::Vector2f center = c.center();
    const float r = c.s * 0.42f;
    const float outline = thickness(c.s, 0.04f);
    const sf::Vector2f shadowCenter = center + sf::Vector2f{c.s * 0.05f, c.s * 0.08f};
    m.fan(shadowCenter, rockPoints(shadowCenter, r, c.gx, c.gy), rgb(0x50371a, 77));
    m.fan(center, rockPoints(center, r + outline / 2.f, c.gx, c.gy), rgb(0x3e3b37));
    m.fan(center, rockPoints(center, r - outline / 2.f, c.gx, c.gy), rgb(0x8e8a83));
    const sf::Vector2f darkCenter = center + sf::Vector2f{c.s * 0.08f, c.s * 0.1f};
    m.fan(darkCenter, rockPoints(darkCenter, c.s * 0.26f, c.gx + 3, c.gy), rgb(0x282623, 71));
    m.ellipse(center + sf::Vector2f{-c.s * 0.13f, -c.s * 0.14f}, c.s * 0.12f, c.s * 0.07f, rgb(0xffffff, 56), -0.5f,
              16);
}

void pillarBarrel(Mesh& m, const Cell& c)
{
    const sf::Vector2f center = c.center();
    const float r = c.s * 0.42f;
    const float outline = thickness(c.s, 0.035f);
    m.circle(center + sf::Vector2f{c.s * 0.05f, c.s * 0.07f}, r, rgb(0x000000, 82), 28);
    m.circle(center, r + outline / 2.f, rgb(0x2a180c), 28);
    const float body = r - outline / 2.f;
    m.circle(center, body, rgb(0x8a5a2e), 28);
    for (int i = -2; i <= 2; ++i)
    {
        const float dx = static_cast<float>(i) * r * 0.38f;
        const float half = std::sqrt(std::max(0.f, body * body - dx * dx));
        m.line({center.x + dx, center.y - half}, {center.x + dx, center.y + half}, thickness(c.s, 0.02f),
               rgb(0x321e0c, 115));
    }
    m.ring(center, r * 0.93f, thickness(c.s, 0.055f), rgb(0x3b3b3f));
    m.ring(center, r * 0.58f, thickness(c.s, 0.055f), rgb(0x3b3b3f));
    m.circle(center + sf::Vector2f{-r * 0.3f, -r * 0.3f}, r * 0.18f, rgb(0xffe6b4, 46), 12);
}

// =========================
// Shared cargo crate (destructible block), independent of the theme
// =========================
void crate(Mesh& m, const Cell& c)
{
    const float s = c.s;
    const float margin = s * 0.09f;
    const float X = c.x + margin;
    const float Y = c.y + margin;
    const float S = s - 2.f * margin;
    const float w = thickness(s, 0.035f);
    const float radius = s * 0.04f;
    const sf::Color ink = rgb(0x3b2414);

    m.polygon(roundedRectPoints(X + s * 0.04f, Y + s * 0.06f, S, S, radius), rgb(0x000000, 77));
    m.polygon(roundedRectPoints(X - w / 2.f, Y - w / 2.f, S + w, S + w, radius + w / 2.f), ink);
    m.polygon(roundedRectPoints(X + w / 2.f, Y + w / 2.f, S - w, S - w, radius - w / 2.f), rgb(0xa8723d));

    // Planks inside the frame.
    const float f = S * 0.15f;
    const float ix = X + f;
    const float iy = Y + f;
    const float iS = S - 2.f * f;
    m.rect(ix - w / 2.f, iy - w / 2.f, iS + w, iS + w, ink);
    const float px = ix + w / 2.f;
    const float py = iy + w / 2.f;
    const float pS = iS - w;
    m.rect(px, py, pS, pS, rgb(0xc48a4d));
    for (int k = 1; k < 3; ++k)
        m.rect(px, py + static_cast<float>(k) * pS / 3.f - w / 2.f, pS, w, rgb(0x502d12, 140));
    for (int k = 0; k < 3; ++k)
        m.line({px + pS * 0.08f, py + pS * (static_cast<float>(k) + 0.5f) / 3.f},
               {px + pS * 0.92f, py + pS * (static_cast<float>(k) + 0.45f) / 3.f}, thickness(s, 0.012f),
               rgb(0x6e411c, 77));

    // Diagonal brace (bottom-left to top-right), clipped to the plank area.
    const float k = S * 0.17f / 2.f * std::sqrt(2.f);
    m.polygon({{px, py + pS - k}, {px + pS - k, py}, {px + pS, py}, {px + pS, py + k}, {px + k, py + pS}, {px, py + pS}},
              rgb(0x93602f));
    m.line({px, py + pS - k}, {px + pS - k, py}, w, ink);
    m.line({px + k, py + pS}, {px + pS, py + k}, w, ink);

    m.rect(X + w, Y + w, S - 2.f * w, f * 0.35f, rgb(0xffe1aa, 56));

    // Iron corner brackets and nails.
    const float L = S * 0.2f;
    const float t = S * 0.055f;
    const sf::Color iron = rgb(0x4a4a50);
    m.rect(X, Y, L, t, iron);
    m.rect(X, Y, t, L, iron);
    m.rect(X + S - L, Y, L, t, iron);
    m.rect(X + S - t, Y, t, L, iron);
    m.rect(X, Y + S - t, L, t, iron);
    m.rect(X, Y + S - L, t, L, iron);
    m.rect(X + S - L, Y + S - t, L, t, iron);
    m.rect(X + S - t, Y + S - L, t, L, iron);
    for (const sf::Vector2f nail : {sf::Vector2f{ix - f / 2.f, iy - f / 2.f}, sf::Vector2f{ix + iS + f / 2.f, iy - f / 2.f},
                                    sf::Vector2f{ix - f / 2.f, iy + iS + f / 2.f},
                                    sf::Vector2f{ix + iS + f / 2.f, iy + iS + f / 2.f}})
        m.circle(nail, s * 0.026f, rgb(0x2a1a0d), 8);
}

// =========================
// Theme dispatch
// =========================
void paintFloor(Mesh& m, const Cell& c, MapTheme theme)
{
    switch (theme)
    {
    case MapTheme::Deck:
        return floorDeck(m, c);
    case MapTheme::Island:
        return floorSand(m, c);
    case MapTheme::Cave:
        return floorStone(m, c);
    case MapTheme::Docks:
        return floorDock(m, c);
    }
}

void paintBorder(Mesh& m, const Cell& c, MapTheme theme)
{
    switch (theme)
    {
    case MapTheme::Deck:
        return wallBulwark(m, c);
    case MapTheme::Island:
        return wallSea(m, c);
    case MapTheme::Cave:
        return wallCave(m, c, true);
    case MapTheme::Docks:
        return wallWater(m, c);
    }
}

void paintPillar(Mesh& m, const Cell& c, MapTheme theme)
{
    switch (theme)
    {
    case MapTheme::Deck:
        return pillarMast(m, c);
    case MapTheme::Island:
        return pillarRock(m, c);
    case MapTheme::Cave:
        return wallCave(m, c, false);
    case MapTheme::Docks:
        return pillarBarrel(m, c);
    }
}

sf::Color gridLineColor(MapTheme theme)
{
    switch (theme)
    {
    case MapTheme::Deck:
        return rgb(0x3c200a, 56);
    case MapTheme::Island:
        return rgb(0x785528, 56);
    case MapTheme::Cave:
        return rgb(0xffffff, 18);
    case MapTheme::Docks:
        return rgb(0x141e23, 64);
    }
    return rgb(0x000000, 40);
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
GridRenderer::GridRenderer(float tileSize, MapTheme theme)
    : m_tileSize(tileSize),
      m_theme(theme),
      m_pixelSize(0u, 0u),
      m_vertices(sf::PrimitiveType::Triangles)
{
}

// =========================
// Public Methods
// =========================
void GridRenderer::update(const Grid& grid)
{
    m_vertices.clear();
    const int width = grid.getWidth();
    const int height = grid.getHeight();
    m_pixelSize = {static_cast<unsigned int>(std::lround(static_cast<float>(width) * m_tileSize)),
                   static_cast<unsigned int>(std::lround(static_cast<float>(height) * m_tileSize))};

    Mesh mesh(m_vertices);
    const auto typeAt = [&](int x, int y) {
        return grid.isInside({x, y}) ? grid.getTile({x, y}).getType() : TileType::Wall;
    };
    const auto onEdge = [&](int x, int y) { return x == 0 || y == 0 || x == width - 1 || y == height - 1; };
    const auto cellAt = [&](int x, int y) {
        return Cell{static_cast<float>(x) * m_tileSize, static_cast<float>(y) * m_tileSize, m_tileSize, x, y};
    };

    // 1. Floors everywhere except on the border walls, which get the frame art.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (typeAt(x, y) == TileType::Wall && onEdge(x, y))
                paintBorder(mesh, cellAt(x, y), m_theme);
            else
                paintFloor(mesh, cellAt(x, y), m_theme);
        }

    // 2. Light grid over every playable cell.
    const sf::Color gridColor = gridLineColor(m_theme);
    const float line = thickness(m_tileSize, 0.012f);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            if (typeAt(x, y) == TileType::Wall)
                continue;
            const Cell c = cellAt(x, y);
            mesh.rect(c.x, c.y, c.s, line, gridColor);
            mesh.rect(c.x, c.y + c.s - line, c.s, line, gridColor);
            mesh.rect(c.x, c.y + line, line, c.s - 2.f * line, gridColor);
            mesh.rect(c.x + c.s - line, c.y + line, line, c.s - 2.f * line, gridColor);
        }

    // 3. Island: foam where the sea touches the sand.
    if (m_theme == MapTheme::Island)
    {
        const std::array<sf::Vector2i, 4> sides = {sf::Vector2i{0, -1}, sf::Vector2i{0, 1}, sf::Vector2i{-1, 0},
                                                    sf::Vector2i{1, 0}};
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                if (typeAt(x, y) != TileType::Wall || !onEdge(x, y))
                    continue;
                const Cell c = cellAt(x, y);
                for (const sf::Vector2i side : sides)
                {
                    const int nx = x + side.x;
                    const int ny = y + side.y;
                    if (!grid.isInside({nx, ny}) || typeAt(nx, ny) == TileType::Wall)
                        continue;
                    for (int k = 0; k < 4; ++k)
                    {
                        const float t = (static_cast<float>(k) + 0.5f) / 4.f;
                        const float u = side.x == 0 ? t : (side.x > 0 ? 1.f : 0.f);
                        const float v = side.y == 0 ? t : (side.y > 0 ? 1.f : 0.f);
                        mesh.circle(c.at(u, v), c.s * 0.09f, rgb(0xffffff, 153), 10);
                    }
                }
            }
    }

    // 4. Pillars and crates on top of the floor.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const TileType type = typeAt(x, y);
            if (type == TileType::Wall && !onEdge(x, y))
                paintPillar(mesh, cellAt(x, y), m_theme);
            else if (type == TileType::DestructibleBlock)
                crate(mesh, cellAt(x, y));
        }

    // 5. Cave: torches on the border walls, lighting the floor inwards.
    if (m_theme == MapTheme::Cave)
    {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const bool corner = (x == 0 || x == width - 1) && (y == 0 || y == height - 1);
                const bool horizontalSide = y == 0 || y == height - 1;
                if (!onEdge(x, y) || corner || typeAt(x, y) != TileType::Wall
                    || (horizontalSide ? x % 4 != 2 : y % 4 != 2))
                    continue;
                const Cell c = cellAt(x, y);
                const sf::Vector2f flame = c.at(0.5f, 0.45f);
                const float inward = y == 0 ? Pi / 2.f : y == height - 1 ? -Pi / 2.f : x == 0 ? 0.f : Pi;
                mesh.glowSector(flame, c.s * 2.2f, sf::Color(255, 170, 70, 82), inward - Pi / 2.f, inward + Pi / 2.f);
                mesh.rect(flame.x - c.s * 0.05f, flame.y, c.s * 0.1f, c.s * 0.3f, rgb(0x4a3020));
                mesh.circle({flame.x, flame.y - c.s * 0.02f}, c.s * 0.12f, rgb(0xff8a1f), 14);
                mesh.circle({flame.x, flame.y + c.s * 0.01f}, c.s * 0.06f, rgb(0xffd36a), 10);
            }
    }
}

float GridRenderer::getTileSize() const noexcept
{
    return m_tileSize;
}

MapTheme GridRenderer::getTheme() const noexcept
{
    return m_theme;
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
// Private Methods
// =========================
void GridRenderer::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    states.transform *= getTransform();
    target.draw(m_vertices, states);
}

} // namespace Bomberman

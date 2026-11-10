#include <cmath>

#include <gtest/gtest.h>

#include "character_select_renderer.h"

using Bomberman::AllCharacterModels;
using Bomberman::CharacterModel;
using Bomberman::CharacterSelectRenderer;

namespace
{

sf::FloatRect bounds(const sf::VertexArray& vertices)
{
    sf::Vector2f min = vertices[0].position;
    sf::Vector2f max = min;
    for (std::size_t i = 1; i < vertices.getVertexCount(); ++i)
    {
        min.x = std::min(min.x, vertices[i].position.x);
        min.y = std::min(min.y, vertices[i].position.y);
        max.x = std::max(max.x, vertices[i].position.x);
        max.y = std::max(max.y, vertices[i].position.y);
    }
    return {min, max - min};
}

// Bounds of a placed character, in design units.
sf::FloatRect characterBounds(const Bomberman::CharacterRenderer& renderer)
{
    sf::FloatRect box = bounds(renderer.getVertices());
    box.position += renderer.getPosition();
    return box;
}

} // namespace

TEST(CharacterSelectRendererTest, WrapLoopsThroughTheFiveModels)
{
    EXPECT_EQ(CharacterSelectRenderer::wrap(0, 1), 1);
    EXPECT_EQ(CharacterSelectRenderer::wrap(4, 1), 0);
    EXPECT_EQ(CharacterSelectRenderer::wrap(0, -1), 4);
    EXPECT_EQ(CharacterSelectRenderer::wrap(2, -7), 0);
}

TEST(CharacterSelectRendererTest, TheViewFitsTheDesignInAnyScreen)
{
    // 16:9: exact fit.
    const sf::View wide = CharacterSelectRenderer::viewFor({1920u, 1080u});
    EXPECT_EQ(wide.getCenter(), sf::Vector2f(800.f, 450.f));
    EXPECT_NEAR(wide.getSize().x, 1600.f, 1e-3f);
    EXPECT_NEAR(wide.getSize().y, 900.f, 1e-3f);

    // 16:10 (MacBook): black bars above and below, the width still fits.
    const sf::View mac = CharacterSelectRenderer::viewFor({2880u, 1800u});
    EXPECT_NEAR(mac.getSize().x, 1600.f, 1e-3f);
    EXPECT_GT(mac.getSize().y, 900.f);
    EXPECT_NEAR(mac.getSize().x / mac.getSize().y, 2880.f / 1800.f, 1e-3f);

    // 4:3, narrower: bars above and below.
    const sf::View square = CharacterSelectRenderer::viewFor({1024u, 768u});
    EXPECT_NEAR(square.getSize().x, 1600.f, 1e-3f);
    EXPECT_NEAR(square.getSize().y, 1200.f, 1e-3f);

    // 21:9, wider: bars on the sides, the height still fits.
    const sf::View ultraWide = CharacterSelectRenderer::viewFor({2560u, 1080u});
    EXPECT_GT(ultraWide.getSize().x, 1600.f);
    EXPECT_NEAR(ultraWide.getSize().y, 900.f, 1e-3f);
}

TEST(CharacterSelectRendererTest, StartsOnTheBuccaneer)
{
    const CharacterSelectRenderer screen;

    EXPECT_EQ(screen.getSelected(), CharacterModel::Buccaneer);
    ASSERT_EQ(screen.getCharacters().size(), AllCharacterModels.size());
    for (std::size_t i = 0; i < AllCharacterModels.size(); ++i)
        EXPECT_EQ(screen.getCharacters()[i].getModel(), AllCharacterModels[i]);
    EXPECT_GT(screen.getDecor().getVertexCount(), 0u);
}

TEST(CharacterSelectRendererTest, TheDeckCoversTheDesignSpace)
{
    const CharacterSelectRenderer screen;
    const sf::FloatRect deck = bounds(screen.getDecor());

    EXPECT_FLOAT_EQ(deck.position.x, 0.f);
    EXPECT_FLOAT_EQ(deck.position.y, 0.f);
    EXPECT_FLOAT_EQ(deck.size.x, CharacterSelectRenderer::DesignSize.x);
    EXPECT_FLOAT_EQ(deck.size.y, CharacterSelectRenderer::DesignSize.y);
}

TEST(CharacterSelectRendererTest, TheSelectedPirateStandsForwardBiggerAndBright)
{
    for (std::size_t selected = 0; selected < AllCharacterModels.size(); ++selected)
    {
        CharacterSelectRenderer screen;
        screen.update(AllCharacterModels[selected]);

        // Mock-up slots: x = 250 + 275 * index; the selected body at y = 520.
        EXPECT_NEAR(screen.getSelectedCenter().x, 250.f + 275.f * static_cast<float>(selected), 1e-3f);
        EXPECT_NEAR(screen.getSelectedCenter().y, 520.f, 1e-3f);

        const auto& characters = screen.getCharacters();
        const sf::FloatRect chosen = characterBounds(characters[selected]);
        for (std::size_t i = 0; i < characters.size(); ++i)
        {
            if (i == selected)
            {
                EXPECT_FLOAT_EQ(characters[i].getBrightness(), 1.f);
                continue;
            }
            EXPECT_FLOAT_EQ(characters[i].getBrightness(), CharacterSelectRenderer::DimmedBrightness);
            // 300 vs 200 design units: the selected one is 1.5 times bigger.
            const sf::FloatRect other = characterBounds(characters[i]);
            EXPECT_GT(chosen.size.y, other.size.y * 1.2f);
            // Every pirate stays on screen.
            EXPECT_GE(other.position.x, 0.f);
            EXPECT_LE(other.position.x + other.size.x, CharacterSelectRenderer::DesignSize.x);
        }
    }
}

TEST(CharacterSelectRendererTest, TheSelectedPirateHops)
{
    CharacterSelectRenderer screen;
    screen.update(CharacterModel::Parrot);
    const float rest = screen.getSelectedCenter().y;

    // Top of the hop: |sin(t * pi * 1.6)| = 1 at t = 1 / 3.2 s.
    screen.update(CharacterModel::Parrot, 1.f / (2.f * CharacterSelectRenderer::HopFrequency));
    EXPECT_NEAR(screen.getSelectedCenter().y, rest - CharacterSelectRenderer::HopHeight, 1e-2f);

    // And back on the deck half a hop later.
    screen.update(CharacterModel::Parrot, 1.f / (2.f * CharacterSelectRenderer::HopFrequency));
    EXPECT_NEAR(screen.getSelectedCenter().y, rest, 1e-2f);
}

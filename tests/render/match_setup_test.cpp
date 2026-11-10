#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "grid_loader.h"
#include "match_setup.h"
#include "match_setup_renderer.h"

using Bomberman::AllCharacterModels;
using Bomberman::CharacterModel;
using Bomberman::CharacterRenderer;
using Bomberman::GameMode;
using Bomberman::Grid;
using Bomberman::MatchSetup;
using Bomberman::MatchSetupRenderer;

namespace
{

using Row = MatchSetup::Row;

constexpr sf::Color Gold(212, 169, 74);

std::vector<Grid> shippedMaps()
{
    std::vector<Grid> maps;
    for (int i = 1; i <= 4; ++i)
        maps.push_back(Bomberman::GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_0"
                                                           + std::to_string(i) + ".json"));
    return maps;
}

MatchSetup setupWithMaps(int mapCount = 4)
{
    MatchSetup setup;
    setup.mapCount = mapCount;
    return setup;
}

sf::FloatRect bounds(const sf::VertexArray& vertices, sf::Vector2f offset = {})
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
    return {min + offset, max - min};
}

bool inside(const sf::FloatRect& box, const sf::FloatRect& area)
{
    return box.position.x >= area.position.x - 0.5f && box.position.y >= area.position.y - 0.5f
        && box.position.x + box.size.x <= area.position.x + area.size.x + 0.5f
        && box.position.y + box.size.y <= area.position.y + area.size.y + 0.5f;
}

// Whether a gold vertex lies on the panel's outline near its top-left
// corner (the frame is a stroke: its vertices sit at the rounded corners).
bool hasGoldFrame(const MatchSetupRenderer& menu, Row row)
{
    const sf::FloatRect panel = MatchSetupRenderer::rowBounds(row);
    const sf::VertexArray& decor = menu.getDecor();
    for (std::size_t i = 0; i < decor.getVertexCount(); ++i)
    {
        const sf::Vertex& v = decor[i];
        if (v.color == Gold && std::abs(v.position.y - panel.position.y) < 4.f
            && v.position.x > panel.position.x - 4.f && v.position.x < panel.position.x + 25.f)
            return true;
    }
    return false;
}

const CharacterRenderer* find(const MatchSetupRenderer& menu, CharacterModel model, float brightness)
{
    for (const CharacterRenderer& character : menu.getCharacters())
        if (character.getModel() == model && character.getBrightness() == brightness)
            return &character;
    return nullptr;
}

int countModel(const MatchSetupRenderer& menu, CharacterModel model)
{
    const std::vector<CharacterRenderer>& characters = menu.getCharacters();
    return static_cast<int>(std::count_if(characters.begin(), characters.end(),
                                          [model](const CharacterRenderer& c) { return c.getModel() == model; }));
}

} // namespace

// =========================
// MatchSetup (menu logic)
// =========================
TEST(MatchSetupTest, DefaultsToANormalGameAgainstThreeComputerPirates)
{
    const MatchSetup setup;

    EXPECT_EQ(setup.mode, GameMode::VersusComputer);
    EXPECT_EQ(setup.pirate, CharacterModel::Buccaneer);
    EXPECT_EQ(setup.map, 0);
    EXPECT_EQ(setup.difficulty, 1); // normal
    EXPECT_EQ(setup.opponents, MatchSetup::MaxOpponents);
    EXPECT_EQ(setup.focus, Row::Pirate);
}

TEST(MatchSetupTest, TheFocusWrapsAroundTheRows)
{
    MatchSetup setup = setupWithMaps();
    const std::vector<Row> down = {Row::Map, Row::Difficulty, Row::Opponents, Row::Mode, Row::Pirate};
    for (const Row expected : down)
    {
        setup.moveFocus(1);
        EXPECT_EQ(setup.focus, expected);
    }
    setup.moveFocus(-2);
    EXPECT_EQ(setup.focus, Row::Opponents);
}

TEST(MatchSetupTest, ChangingOnlyAffectsTheFocusedRow)
{
    MatchSetup setup = setupWithMaps();
    setup.focus = Row::Map;
    setup.change(1);

    EXPECT_EQ(setup.map, 1);
    EXPECT_EQ(setup.pirate, CharacterModel::Buccaneer);
    EXPECT_EQ(setup.difficulty, 1);
    EXPECT_EQ(setup.opponents, 3);
    EXPECT_EQ(setup.mode, GameMode::VersusComputer);
}

TEST(MatchSetupTest, EveryRowWrapsAround)
{
    MatchSetup setup = setupWithMaps();

    setup.focus = Row::Pirate;
    for (std::size_t i = 1; i <= AllCharacterModels.size(); ++i)
    {
        setup.change(1);
        EXPECT_EQ(setup.pirate, AllCharacterModels[i % AllCharacterModels.size()]);
    }
    setup.change(-1);
    EXPECT_EQ(setup.pirate, CharacterModel::Revenant);

    setup.focus = Row::Map;
    setup.change(-1);
    EXPECT_EQ(setup.map, 3);
    setup.change(1);
    EXPECT_EQ(setup.map, 0);

    setup.focus = Row::Difficulty;
    setup.change(1);
    EXPECT_EQ(setup.difficulty, 2);
    setup.change(1);
    EXPECT_EQ(setup.difficulty, 0);

    setup.focus = Row::Opponents;
    setup.change(1);
    EXPECT_EQ(setup.opponents, 1);
    setup.change(-1);
    EXPECT_EQ(setup.opponents, 3);
    setup.change(-2);
    EXPECT_EQ(setup.opponents, 1);

    setup.focus = Row::Mode;
    setup.change(1);
    EXPECT_EQ(setup.mode, GameMode::LocalMultiplayer);
    setup.change(1);
    EXPECT_EQ(setup.mode, GameMode::VersusComputer);
}

TEST(MatchSetupTest, OnlyTheComputerModeCanStartAndItNeedsAMap)
{
    MatchSetup setup = setupWithMaps();
    EXPECT_TRUE(setup.canStart());

    setup.mode = GameMode::LocalMultiplayer;
    EXPECT_FALSE(setup.canStart());

    MatchSetup noMap = setupWithMaps(0);
    EXPECT_FALSE(noMap.canStart());
    noMap.focus = Row::Map;
    noMap.change(1);
    EXPECT_EQ(noMap.map, 0);
}

TEST(MatchSetupTest, OpponentsAreTheRestOfTheCrewInOrder)
{
    EXPECT_EQ(Bomberman::opponentModels(CharacterModel::Buccaneer, 3),
              (std::vector<CharacterModel>{CharacterModel::Captain, CharacterModel::Lookout, CharacterModel::Parrot}));
    EXPECT_EQ(Bomberman::opponentModels(CharacterModel::Lookout, 3),
              (std::vector<CharacterModel>{CharacterModel::Buccaneer, CharacterModel::Captain, CharacterModel::Parrot}));
    EXPECT_EQ(Bomberman::opponentModels(CharacterModel::Captain, 1),
              (std::vector<CharacterModel>{CharacterModel::Buccaneer}));
    for (const CharacterModel player : AllCharacterModels)
    {
        const std::vector<CharacterModel> models = Bomberman::opponentModels(player, MatchSetup::MaxOpponents);
        EXPECT_EQ(models.size(), 3u);
        EXPECT_EQ(std::count(models.begin(), models.end(), player), 0);
    }
}

// =========================
// MatchSetupRenderer
// =========================
TEST(MatchSetupRendererTest, TheViewFitsTheDesignInAnyScreen)
{
    const sf::View wide = MatchSetupRenderer::viewFor({1920u, 1080u});
    EXPECT_EQ(wide.getCenter(), sf::Vector2f(800.f, 450.f));
    EXPECT_NEAR(wide.getSize().x, 1600.f, 1e-3f);
    EXPECT_NEAR(wide.getSize().y, 900.f, 1e-3f);

    const sf::View mac = MatchSetupRenderer::viewFor({2880u, 1800u});
    EXPECT_NEAR(mac.getSize().x, 1600.f, 1e-3f);
    EXPECT_GT(mac.getSize().y, 900.f);
}

TEST(MatchSetupRendererTest, RowsDoNotOverlapAndStayOnScreen)
{
    const sf::FloatRect screen({0.f, 0.f}, MatchSetupRenderer::DesignSize);
    for (int a = 0; a < MatchSetup::RowCount; ++a)
    {
        const sf::FloatRect first = MatchSetupRenderer::rowBounds(static_cast<Row>(a));
        EXPECT_TRUE(inside(first, screen));
        for (int b = a + 1; b < MatchSetup::RowCount; ++b)
            EXPECT_FALSE(first.findIntersection(MatchSetupRenderer::rowBounds(static_cast<Row>(b))).has_value())
                << a << " / " << b;
    }
}

TEST(MatchSetupRendererTest, EverythingIsDrawnInsideTheScreen)
{
    MatchSetupRenderer menu(shippedMaps());
    const sf::FloatRect screen({0.f, 0.f}, MatchSetupRenderer::DesignSize);
    for (const CharacterModel pirate : AllCharacterModels)
    {
        MatchSetup setup = setupWithMaps();
        setup.pirate = pirate;
        menu.update(setup, 0.3f);
        EXPECT_TRUE(inside(bounds(menu.getDecor()), screen));
        EXPECT_TRUE(inside(bounds(menu.getOverlay()), screen));
        EXPECT_TRUE(inside(bounds(menu.getFront()), screen));
        for (const CharacterRenderer& character : menu.getCharacters())
            EXPECT_TRUE(inside(bounds(character.getVertices(), character.getPosition()), screen));
    }
}

TEST(MatchSetupRendererTest, OnlyTheFocusedRowHasAGoldenFrame)
{
    MatchSetupRenderer menu(shippedMaps());
    for (int focused = 0; focused < MatchSetup::RowCount; ++focused)
    {
        MatchSetup setup = setupWithMaps();
        setup.focus = static_cast<Row>(focused);
        menu.update(setup);
        for (int row = 0; row < MatchSetup::RowCount; ++row)
            EXPECT_EQ(hasGoldFrame(menu, static_cast<Row>(row)), row == focused) << focused << " / " << row;
    }
}

TEST(MatchSetupRendererTest, TheChosenPirateStandsBiggerInItsSlotAndIsDrawnLast)
{
    MatchSetupRenderer menu(shippedMaps());
    MatchSetup setup = setupWithMaps();
    setup.pirate = CharacterModel::Parrot;
    menu.update(setup);

    const CharacterRenderer& chosen = menu.getCharacters().back();
    EXPECT_EQ(chosen.getModel(), CharacterModel::Parrot);
    EXPECT_FLOAT_EQ(chosen.getBrightness(), 1.f);
    EXPECT_NEAR(menu.getSelectedPirateCenter().x, 1085.f, 1e-3f); // fourth slot

    const float chosenWidth = bounds(chosen.getVertices()).size.x;

    // The same parrot, not chosen: dimmed and smaller.
    setup.pirate = CharacterModel::Buccaneer;
    menu.update(setup);
    const CharacterRenderer* dimmed = find(menu, CharacterModel::Parrot, MatchSetupRenderer::DimmedBrightness);
    ASSERT_NE(dimmed, nullptr);
    EXPECT_LT(bounds(dimmed->getVertices()).size.x, chosenWidth);
}

TEST(MatchSetupRendererTest, TheChosenPirateHopsWithoutSinking)
{
    MatchSetupRenderer menu;
    const MatchSetup setup;
    float highest = 1e9f;
    for (int frame = 0; frame < 60; ++frame)
    {
        menu.update(setup, 1.f / 60.f);
        const float y = menu.getSelectedPirateCenter().y;
        EXPECT_LE(y, 315.f + 1e-3f);
        EXPECT_GE(y, 315.f - MatchSetupRenderer::HopHeight - 1e-3f);
        highest = std::min(highest, y);
    }
    EXPECT_LT(highest, 315.f - MatchSetupRenderer::HopHeight / 2.f);
}

TEST(MatchSetupRendererTest, EachMapHasAMiniatureFittingItsSlot)
{
    MatchSetupRenderer menu(shippedMaps());
    menu.update(setupWithMaps());

    ASSERT_EQ(menu.getMaps().size(), 4u);
    for (std::size_t i = 0; i < 4; ++i)
    {
        const Bomberman::GridRenderer& miniature = menu.getMaps()[i];
        const sf::Vector2f size(static_cast<float>(miniature.getPixelSize().x),
                                static_cast<float>(miniature.getPixelSize().y));
        EXPECT_LE(size.x, MatchSetupRenderer::MapSlotSize.x + 0.5f);
        EXPECT_LE(size.y, MatchSetupRenderer::MapSlotSize.y + 0.5f);
        EXPECT_TRUE(inside(sf::FloatRect(miniature.getPosition(), size), MatchSetupRenderer::rowBounds(Row::Map)));
    }
    // Left to right, in the menu's order.
    EXPECT_LT(menu.getMaps()[0].getPosition().x, menu.getMaps()[1].getPosition().x);
}

TEST(MatchSetupRendererTest, TheLaunchButtonIsOffWhenTheSetupCannotStart)
{
    MatchSetupRenderer menu(shippedMaps());
    MatchSetup setup = setupWithMaps();
    menu.update(setup);
    EXPECT_TRUE(menu.isLaunchEnabled());

    setup.mode = GameMode::LocalMultiplayer;
    menu.update(setup);
    EXPECT_FALSE(menu.isLaunchEnabled());

    MatchSetupRenderer empty;
    empty.update(MatchSetup{});
    EXPECT_FALSE(empty.isLaunchEnabled());
}

TEST(MatchSetupRendererTest, OpponentSlotsShowOneTwoAndThreeSkeletons)
{
    MatchSetupRenderer menu(shippedMaps());
    MatchSetup setup = setupWithMaps();
    setup.pirate = CharacterModel::Buccaneer;
    menu.update(setup);

    // 2 on the row badge, 1 in the solo icon, 1 in the pirate row and
    // 1 + 2 + 3 in the opponent slots.
    EXPECT_EQ(countModel(menu, CharacterModel::Revenant), 2 + 1 + 1 + 6);
    // Solo icon, the two multiplayer icons, the pirate row.
    EXPECT_EQ(countModel(menu, CharacterModel::Buccaneer), 1 + 2 + 1);
}

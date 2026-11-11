#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "discovery_renderer.h"
#include "grid_loader.h"
#include "lobby_renderer.h"
#include "menu_art.h"

using Bomberman::CharacterModel;
using Bomberman::CharacterRenderer;
using Bomberman::DiscoveryCard;
using Bomberman::DiscoveryRenderer;
using Bomberman::Grid;
using Bomberman::LobbyRenderer;
using Bomberman::LobbyView;

// The network screens of the validated mock-up "Rassembler l'équipage":
// the waiting room (proposal A, "Le pont d'embarquement") and the search
// for a ship. Geometry only (no GL context).
namespace
{

Grid level(int i)
{
    return Bomberman::GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_0" + std::to_string(i)
                                               + ".json");
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

const sf::FloatRect Screen({0.f, 0.f}, {1600.f, 900.f});

LobbyView twoPlayers()
{
    LobbyView view;
    view.seats[0] = CharacterModel::Captain;
    view.seats[2] = CharacterModel::Parrot;
    view.localSeat = 0;
    view.isHost = true;
    view.canStart = true;
    return view;
}

DiscoveryCard card(const std::string& key, CharacterModel model, int taken, int levelIndex = 1)
{
    return DiscoveryCard{key, model, taken, 4, level(levelIndex)};
}

} // namespace

// =========================
// Waiting room
// =========================
TEST(LobbyRendererTest, EachTakenSeatShowsItsPirateOnItsPanel)
{
    LobbyRenderer lobby(level(1));
    lobby.update(twoPlayers(), 1.f); // past the pop-in

    const std::vector<CharacterRenderer>& pirates = lobby.getCharacters();
    ASSERT_EQ(pirates.size(), 2u);
    EXPECT_EQ(pirates[0].getModel(), CharacterModel::Captain);
    EXPECT_EQ(pirates[1].getModel(), CharacterModel::Parrot);
    for (const int seat : {0, 2})
    {
        ASSERT_TRUE(lobby.getPirateCenter(seat).has_value());
        EXPECT_TRUE(LobbyRenderer::seatBounds(seat).contains(*lobby.getPirateCenter(seat)));
    }
    EXPECT_FALSE(lobby.getPirateCenter(1).has_value());
    EXPECT_FALSE(lobby.getPirateCenter(3).has_value());

    // Every pirate stays inside its panel.
    for (std::size_t i = 0; i < pirates.size(); ++i)
        EXPECT_TRUE(inside(bounds(pirates[i].getVertices(), pirates[i].getPosition()),
                           LobbyRenderer::seatBounds(i == 0 ? 0 : 2)));
}

TEST(LobbyRendererTest, TheLocalPirateHopsTheOthersStandStill)
{
    LobbyRenderer lobby(level(1));
    LobbyView view = twoPlayers();
    view.localSeat = 2;
    float highest = 1e9f;
    float lowest = -1e9f;
    for (int frame = 0; frame < 60; ++frame)
    {
        lobby.update(view, 1.f / 60.f);
        highest = std::min(highest, lobby.getPirateCenter(2)->y);
        lowest = std::max(lowest, lobby.getPirateCenter(2)->y);
        EXPECT_FLOAT_EQ(lobby.getPirateCenter(0)->y, 345.f);
    }
    EXPECT_GT(lowest - highest, 10.f);
}

TEST(LobbyRendererTest, ANewcomerPopsIn)
{
    LobbyRenderer lobby(level(1));
    LobbyView view = twoPlayers();
    view.seats[2].reset();
    lobby.update(view, 1.f);

    view.seats[2] = CharacterModel::Parrot;
    lobby.update(view, 1.f / 60.f);
    const float small = bounds(lobby.getCharacters()[1].getVertices()).size.x;
    lobby.update(view, LobbyRenderer::PopDuration);
    const float full = bounds(lobby.getCharacters()[1].getVertices()).size.x;
    EXPECT_LT(small, full * 0.5f);

    // Changing pirate does not pop.
    view.seats[2] = CharacterModel::Lookout;
    lobby.update(view, 1.f / 60.f);
    EXPECT_GT(bounds(lobby.getCharacters()[1].getVertices()).size.x, full * 0.8f);
}

TEST(LobbyRendererTest, OnlyTheHostHasALaunchButton)
{
    LobbyRenderer lobby(level(1));
    LobbyView view = twoPlayers();
    lobby.update(view);
    EXPECT_TRUE(lobby.isLaunchEnabled());

    view.canStart = false;
    lobby.update(view);
    EXPECT_FALSE(lobby.isLaunchEnabled());

    view.canStart = true;
    view.isHost = false; // a client waits for the host (hourglass)
    view.localSeat = 2;
    lobby.update(view);
    EXPECT_FALSE(lobby.isLaunchEnabled());
}

TEST(LobbyRendererTest, EverythingStaysOnScreenAndTheMapFitsItsSlot)
{
    LobbyRenderer lobby(level(3));
    for (const bool host : {true, false})
    {
        LobbyView view = twoPlayers();
        view.isHost = host;
        lobby.update(view, 0.7f);
        EXPECT_TRUE(inside(bounds(lobby.getDecor()), Screen));
        EXPECT_TRUE(inside(bounds(lobby.getFront()), Screen));
    }
    const sf::Vector2f size(static_cast<float>(lobby.getMap().getPixelSize().x),
                            static_cast<float>(lobby.getMap().getPixelSize().y));
    EXPECT_LE(size.x, LobbyRenderer::MapSlotSize.x + 0.5f);
    EXPECT_LE(size.y, LobbyRenderer::MapSlotSize.y + 0.5f);
    EXPECT_TRUE(inside(sf::FloatRect(lobby.getMap().getPosition(), size), sf::FloatRect({40.f, 625.f}, {1520.f, 245.f})));
}

TEST(LobbyRendererTest, PiratesWearTheirSeatColor)
{
    LobbyRenderer lobby(level(1));
    LobbyView view;
    view.seats[0] = CharacterModel::Buccaneer;
    view.seats[1] = CharacterModel::Captain;
    lobby.update(view, 1.f);
    // The buccaneer on seat 0 wears the red bandana of player 0.
    const sf::VertexArray& vertices = lobby.getCharacters()[0].getVertices();
    bool red = false;
    for (std::size_t i = 0; i < vertices.getVertexCount() && !red; ++i)
        red = vertices[i].color == Bomberman::MenuArt::SeatColors[0];
    EXPECT_TRUE(red);
}

// =========================
// Search for a ship
// =========================
TEST(DiscoveryRendererTest, WithoutHostsOnlyTheSpyglassSearches)
{
    DiscoveryRenderer search;
    search.update({}, 0, 0.5f);
    EXPECT_EQ(search.getCardsShown(), 0);
    EXPECT_TRUE(search.getCharacters().empty());
    EXPECT_GT(search.getDecor().getVertexCount(), 0u);
}

TEST(DiscoveryRendererTest, EachHostHasACardWithItsPirateAndItsMap)
{
    DiscoveryRenderer search;
    const std::vector<DiscoveryCard> cards = {card("a", CharacterModel::Captain, 2, 1),
                                              card("b", CharacterModel::Revenant, 1, 3)};
    search.update(cards, 1, 0.2f);

    ASSERT_EQ(search.getCardsShown(), 2);
    ASSERT_EQ(search.getCharacters().size(), 2u);
    EXPECT_EQ(search.getCharacters()[0].getModel(), CharacterModel::Captain);
    EXPECT_EQ(search.getCharacters()[1].getModel(), CharacterModel::Revenant);
    EXPECT_LT(search.getCharacters()[0].getBrightness(), 1.f); // not selected
    EXPECT_FLOAT_EQ(search.getCharacters()[1].getBrightness(), 1.f);

    const std::vector<const Bomberman::GridRenderer*> maps = search.getShownMaps();
    ASSERT_EQ(maps.size(), 2u);
    for (int slot = 0; slot < 2; ++slot)
    {
        const Bomberman::GridRenderer& map = *maps[static_cast<std::size_t>(slot)];
        const sf::Vector2f size(static_cast<float>(map.getPixelSize().x), static_cast<float>(map.getPixelSize().y));
        EXPECT_TRUE(inside(sf::FloatRect(map.getPosition(), size), DiscoveryRenderer::cardBounds(slot)));
    }
    EXPECT_TRUE(inside(bounds(search.getFront()), Screen)); // (the light cone of the decor leaves the screen)
}

TEST(DiscoveryRendererTest, CardsArePagedThreeAtATime)
{
    DiscoveryRenderer search;
    std::vector<DiscoveryCard> cards;
    for (int i = 0; i < 5; ++i)
        cards.push_back(card("host" + std::to_string(i), Bomberman::AllCharacterModels[static_cast<std::size_t>(i)], 1));

    search.update(cards, 1);
    EXPECT_EQ(search.getCardsShown(), DiscoveryRenderer::CardsPerPage);
    search.update(cards, 4);
    EXPECT_EQ(search.getCardsShown(), 2);
    EXPECT_EQ(search.getCharacters().front().getModel(), Bomberman::AllCharacterModels[3]);
}

TEST(DiscoveryRendererTest, AMiniatureIsBuiltOncePerHost)
{
    DiscoveryRenderer search;
    const std::vector<DiscoveryCard> cards = {card("a", CharacterModel::Captain, 1)};
    search.update(cards, 0);
    const Bomberman::GridRenderer* first = search.getShownMaps().front();
    search.update(cards, 0, 0.1f);
    EXPECT_EQ(search.getShownMaps().front(), first);
}

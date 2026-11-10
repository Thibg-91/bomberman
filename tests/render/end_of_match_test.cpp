#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "character.h"
#include "character_renderer.h"
#include "game_over_renderer.h"

using Bomberman::Character;
using Bomberman::CharacterModel;
using Bomberman::CharacterRenderer;
using Bomberman::GameOverRenderer;
using Bomberman::MatchOutcome;

namespace
{

constexpr float TileSize = 120.f;

CharacterRenderer dyingAt(float seconds, CharacterModel model = CharacterModel::Buccaneer)
{
    CharacterRenderer renderer(TileSize, CharacterRenderer::Palette{}, model);
    renderer.triggerDeathAnimation();
    renderer.update(Character({1, 1}), seconds);
    return renderer;
}

int maxChannel(const sf::VertexArray& vertices)
{
    int result = 0;
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
    {
        const sf::Color c = vertices[i].color;
        result = std::max({result, static_cast<int>(c.r), static_cast<int>(c.g), static_cast<int>(c.b)});
    }
    return result;
}

double meanLightness(const sf::VertexArray& vertices)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
        sum += vertices[i].color.r + vertices[i].color.g + vertices[i].color.b;
    return sum / static_cast<double>(std::max<std::size_t>(vertices.getVertexCount(), 1));
}

int countColor(const sf::VertexArray& vertices, sf::Color color, float belowY = 1e9f)
{
    int n = 0;
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
        n += vertices[i].color == color && vertices[i].position.y < belowY ? 1 : 0;
    return n;
}

GameOverRenderer endScreenAt(MatchOutcome outcome, float seconds, CharacterModel winner = CharacterModel::Captain)
{
    GameOverRenderer screen;
    screen.start(outcome, winner);
    screen.update(seconds);
    return screen;
}

} // namespace

// =========================
// Death animation ("Carbonisé")
// =========================
TEST(DeathAnimationTest, NobodyIsDyingUntilItIsTriggered)
{
    CharacterRenderer renderer(TileSize);
    renderer.update(Character({1, 1}), 1.f);
    EXPECT_FALSE(renderer.isDying());
    EXPECT_FALSE(renderer.isDeathAnimationFinished());

    renderer.triggerDeathAnimation();
    EXPECT_TRUE(renderer.isDying());
}

TEST(DeathAnimationTest, AWhiteFlashThenCharredBlack)
{
    CharacterRenderer alive(TileSize);
    alive.update(Character({1, 1}));

    EXPECT_GT(meanLightness(dyingAt(0.05f).getVertices()), meanLightness(alive.getVertices()));
    // Fully charred: every color at 18 %, before the smoke shows up.
    EXPECT_LE(maxChannel(dyingAt(0.45f).getVertices()), 50);
}

TEST(DeathAnimationTest, ItCrumblesDownOntoItsSoles)
{
    const CharacterRenderer standing = dyingAt(0.45f);
    const sf::FloatRect before = standing.getVertices().getBounds();
    const CharacterRenderer crumbling = dyingAt(0.8f);

    // Only the character (no smoke yet at 0.45 s); at 0.8 s the squashed
    // character is lower than the standing one's top.
    float highestOpaque = 1e9f;
    const sf::VertexArray& vertices = crumbling.getVertices();
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
        if (vertices[i].color.r < 60 && vertices[i].color.a > 200)
            highestOpaque = std::min(highestOpaque, vertices[i].position.y);
    EXPECT_GT(highestOpaque, before.position.y + before.size.y * 0.3f);
}

TEST(DeathAnimationTest, ItEndsWithNothingLeftToDraw)
{
    const CharacterRenderer gone = dyingAt(CharacterRenderer::DeathDuration + 1.f);

    EXPECT_FALSE(gone.isDying());
    EXPECT_TRUE(gone.isDeathAnimationFinished());
    const sf::VertexArray& vertices = gone.getVertices();
    for (std::size_t i = 0; i < vertices.getVertexCount(); ++i)
        EXPECT_EQ(vertices[i].color.a, 0);
}

TEST(DeathAnimationTest, TriggeringAgainDoesNotRestartIt)
{
    CharacterRenderer renderer = dyingAt(0.9f);
    renderer.triggerDeathAnimation();
    renderer.update(Character({1, 1}), 0.5f);
    EXPECT_TRUE(renderer.isDeathAnimationFinished());
}

TEST(DeathAnimationTest, EveryModelDiesTheSameWay)
{
    for (const CharacterModel model : Bomberman::AllCharacterModels)
    {
        EXPECT_LE(maxChannel(dyingAt(0.45f, model).getVertices()), 50);
        EXPECT_TRUE(dyingAt(2.f, model).isDeathAnimationFinished());
    }
}

// =========================
// End screen ("Le pavillon noir")
// =========================
TEST(GameOverRendererTest, NothingBeforeItStarts)
{
    GameOverRenderer screen;
    screen.update(1.f);
    EXPECT_FALSE(screen.isStarted());
    EXPECT_FALSE(screen.isFinished());
    EXPECT_EQ(screen.getBack().getVertexCount(), 0u);
}

TEST(GameOverRendererTest, ItLastsThreeSeconds)
{
    GameOverRenderer screen;
    screen.start(MatchOutcome::Victory, CharacterModel::Buccaneer);
    screen.update(2.9f);
    EXPECT_FALSE(screen.isFinished());
    screen.update(0.2f);
    EXPECT_TRUE(screen.isFinished());
    EXPECT_FLOAT_EQ(screen.getElapsed(), GameOverRenderer::Duration);
    EXPECT_FLOAT_EQ(GameOverRenderer::Duration, 3.f);
}

TEST(GameOverRendererTest, TheLettersFallOntoTheFlagOneByOne)
{
    const GameOverRenderer early = endScreenAt(MatchOutcome::Defeat, 0.5f);
    const GameOverRenderer settled = endScreenAt(MatchOutcome::Defeat, 1.5f);

    // G has started falling, the last R has not.
    EXPECT_LT(early.getLetterCenter(7).y, early.getLetterCenter(0).y);
    for (int i = 0; i < GameOverRenderer::LetterCount; ++i)
    {
        const sf::Vector2f c = settled.getLetterCenter(i);
        EXPECT_NEAR(c.y, 70.f + 150.f, 1e-2f) << i; // the middle of the settled flag
        EXPECT_GT(c.x, 230.f);
        EXPECT_LT(c.x, 1370.f);
        if (i > 0)
            EXPECT_GT(c.x, settled.getLetterCenter(i - 1).x);
    }
}

TEST(GameOverRendererTest, GoldLettersOnlyForAVictory)
{
    // On the flag (the chest's coins below share the gold).
    const sf::Color gold(242, 201, 76);
    const float flagBottom = 380.f;
    EXPECT_GT(countColor(endScreenAt(MatchOutcome::Victory, 1.5f).getBack(), gold, flagBottom), 0);
    EXPECT_EQ(countColor(endScreenAt(MatchOutcome::Defeat, 1.5f).getBack(), gold, flagBottom), 0);
}

TEST(GameOverRendererTest, TheWinnerAppearsOnTheChest)
{
    EXPECT_FALSE(endScreenAt(MatchOutcome::Defeat, 0.5f).getWinner().has_value());

    const GameOverRenderer screen = endScreenAt(MatchOutcome::Defeat, 1.5f, CharacterModel::Parrot);
    ASSERT_TRUE(screen.getWinner().has_value());
    EXPECT_EQ(screen.getWinner()->getModel(), CharacterModel::Parrot);
    EXPECT_EQ(screen.getOutcome(), MatchOutcome::Defeat);
}

TEST(GameOverRendererTest, ADrawHasASkullInsteadOfAWinner)
{
    const GameOverRenderer screen = endScreenAt(MatchOutcome::Draw, 1.5f, CharacterModel::Captain);
    EXPECT_FALSE(screen.getWinner().has_value());
    EXPECT_GT(countColor(screen.getFront(), sf::Color(243, 234, 220)), 0); // the skull's bone
}

TEST(GameOverRendererTest, GoldRainsOnlyOnAVictory)
{
    // Same winner, same moment: only the falling coins differ.
    const std::size_t victory = endScreenAt(MatchOutcome::Victory, 1.8f).getFront().getVertexCount();
    const std::size_t defeat = endScreenAt(MatchOutcome::Defeat, 1.8f).getFront().getVertexCount();
    EXPECT_GT(victory, defeat);
}

TEST(GameOverRendererTest, TheViewFitsTheDesignInAnyScreen)
{
    const sf::View view = GameOverRenderer::viewFor({1920u, 1080u});
    EXPECT_EQ(view.getCenter(), sf::Vector2f(800.f, 450.f));
    EXPECT_NEAR(view.getSize().x, 1600.f, 1e-3f);
}

#include "match.h"

#include <algorithm>
#include <cstddef>

#include "bot.h"
#include "bot_profile.h"
#include "map_theme.h"

namespace Bomberman
{

static_assert(MatchSetup::DifficultyCount == BotDifficultyCount, "the menu offers one difficulty per BotDifficulty");

CharacterModel modelAt(int index) noexcept
{
    return AllCharacterModels[static_cast<std::size_t>(std::clamp(index, 0, CharacterModelCount - 1))];
}

int indexOfModel(CharacterModel model) noexcept
{
    const auto it = std::find(AllCharacterModels.begin(), AllCharacterModels.end(), model);
    return static_cast<int>(it - AllCharacterModels.begin());
}

float tileSizeFor(const Grid& grid, sf::Vector2u screenSize) noexcept
{
    return std::min(static_cast<float>(screenSize.x) / static_cast<float>(std::max(grid.getWidth(), 1)),
                    static_cast<float>(screenSize.y) / static_cast<float>(std::max(grid.getHeight(), 1)));
}

namespace
{

// The renderers of an engine whose players are all added.
std::unique_ptr<Match> makeMatch(MatchRole role, GameEngine engine, PlayerId player, std::vector<CharacterModel> models,
                                 std::vector<CharacterRenderer::Palette> palettes, MatchReferee referee,
                                 sf::Vector2u screenSize)
{
    const Grid& grid = engine.getGrid();
    const float tileSize = tileSizeFor(grid, screenSize);
    const MapTheme theme = mapThemeFromName(grid.getTheme());

    std::vector<CharacterRenderer> characterRenderers;
    for (std::size_t id = 0; id < models.size(); ++id)
        characterRenderers.emplace_back(tileSize, palettes[id], models[id]);

    auto match = std::make_unique<Match>(Match{role,
                                               std::move(engine),
                                               player,
                                               GridRenderer(tileSize, theme),
                                               DangerZoneRenderer(tileSize),
                                               UpgradeRenderer(tileSize),
                                               std::move(characterRenderers),
                                               std::move(models),
                                               std::move(palettes),
                                               ExplosionRenderer(tileSize),
                                               referee,
                                               GameOverRenderer{},
                                               {},
                                               TickPlayout(GameRules::DefaultTickRate),
                                               false,
                                               0.f});
    match->playout = TickPlayout(match->engine.getRules().tickRate);
    match->gridRenderer.update(match->engine.getGrid());
    return match;
}

// Shows the end screen for the referee's verdict.
void showGameOver(Match& match)
{
    const std::optional<PlayerId> winner = match.referee.getWinner();
    if (!winner)
    {
        match.gameOver.start(MatchOutcome::Draw);
        return;
    }
    const auto id = static_cast<std::size_t>(*winner);
    match.gameOver.start(*winner == match.player ? MatchOutcome::Victory : MatchOutcome::Defeat, match.models[id],
                         match.palettes[id]);
}

} // namespace

std::unique_ptr<Match> startSoloMatch(const Grid& grid, const MatchSetup& setup, std::uint64_t seed,
                                      sf::Vector2u screenSize)
{
    GameRules rules;
    rules.seed = seed;
    GameEngine engine(grid, rules);
    const PlayerId player = *engine.addPlayer(); // the menu only offers maps with a spawn point
    std::vector<CharacterModel> models = {setup.pirate};
    std::vector<CharacterRenderer::Palette> palettes = {CharacterRenderer::Palette{}};

    const BotDifficulty difficulty = AllBotDifficulties[static_cast<std::size_t>(setup.difficulty)];
    for (const CharacterModel model : opponentModels(setup.pirate, setup.opponents))
    {
        const std::optional<PlayerId> id = engine.addPlayer();
        if (!id)
            break; // no spawn point left
        engine.setController(*id, std::make_unique<Bot>(
                                      difficulty, rules.seed ^ (0x9E3779B97F4A7C15ull * static_cast<std::uint64_t>(*id))));
        models.push_back(model);
        palettes.push_back(CharacterRenderer::paletteForPlayer(*id));
    }
    return makeMatch(MatchRole::Solo, std::move(engine), player, std::move(models), std::move(palettes),
                     MatchReferee(player), screenSize);
}

std::unique_ptr<Match> startNetworkMatch(const StartMatch& start, MatchRole role, sf::Vector2u screenSize)
{
    GameEngine engine(start.grid, start.rules);
    std::vector<CharacterModel> models;
    std::vector<CharacterRenderer::Palette> palettes;
    for (const MatchPlayer& player : start.players)
    {
        if (!engine.addPlayer())
            break; // the host only seats as many players as the map has spawn points
        models.push_back(modelAt(player.model));
        palettes.push_back(CharacterRenderer::paletteForPlayer(player.seat));
    }
    // The host records what its engine executes, to relay it.
    engine.setCommandRecording(role == MatchRole::Host);
    return makeMatch(role, std::move(engine), start.you, std::move(models), std::move(palettes),
                     MatchReferee(start.you, MatchReferee::SpectatorRule::RealTime), screenSize);
}

sf::View centeredView(const Match& match, sf::Vector2u screenSize)
{
    const sf::Vector2f screen(static_cast<float>(screenSize.x), static_cast<float>(screenSize.y));
    const sf::Vector2f board(static_cast<float>(match.gridRenderer.getPixelSize().x),
                             static_cast<float>(match.gridRenderer.getPixelSize().y));
    return sf::View(sf::FloatRect((board - screen) / 2.f, screen));
}

void presentFrame(Match& match, const std::vector<GameEvent>& events, float deltaSeconds)
{
    GameEngine& engine = match.engine;
    for (const GameEvent& event : events)
    {
        switch (event.type)
        {
        case GameEvent::Type::BombPlaced:
            match.characterRenderers[static_cast<std::size_t>(event.playerId)].triggerBombAnimation();
            break;
        case GameEvent::Type::PlayerKilled:
            match.characterRenderers[static_cast<std::size_t>(event.playerId)].triggerDeathAnimation();
            break;
        case GameEvent::Type::Explosion:
            match.gridRenderer.update(engine.getGrid()); // blocks were destroyed
            match.explosionRenderer.add(event.blast);    // fireballs over the blast's cells
            break;
        default:
            break;
        }
    }

    // Upgrades revealed by destroyed crates, picked up by walking on them.
    match.upgradeRenderer.update(engine.getUpgrades(), deltaSeconds);
    // Cells each ticking bomb is about to hit, then the explosions themselves.
    match.dangerZoneRenderer.update(engine.getBombManager().getBombs(), engine.getGrid());
    match.explosionRenderer.update(deltaSeconds);

    for (std::size_t id = 0; id < match.characterRenderers.size(); ++id)
        match.characterRenderers[id].update(engine.getCharacter(static_cast<PlayerId>(id)), deltaSeconds);

    const std::vector<Bomb>& bombs = engine.getBombManager().getBombs();
    match.bombRenderers.assign(bombs.size(), BombRenderer(match.gridRenderer.getTileSize()));
    for (std::size_t i = 0; i < bombs.size(); ++i)
        match.bombRenderers[i].update(bombs[i]);

    // The verdict is taken tick by tick from the deaths (see MatchReferee):
    // the host and its clients always agree on it.
    match.referee.update(engine, deltaSeconds, events);
}

bool drawMatch(sf::RenderWindow& window, Match& match, sf::Vector2u screenSize, float deltaSeconds)
{
    const GameEngine& engine = match.engine;
    window.setView(centeredView(match, screenSize));
    window.draw(match.gridRenderer);
    window.draw(match.dangerZoneRenderer);
    window.draw(match.upgradeRenderer);
    for (const BombRenderer& bombRenderer : match.bombRenderers)
        window.draw(bombRenderer);
    // The dead stay on screen while their death animation plays.
    for (std::size_t id = 0; id < match.characterRenderers.size(); ++id)
        if (engine.getCharacter(static_cast<PlayerId>(id)).isAlive() || match.characterRenderers[id].isDying())
            window.draw(match.characterRenderers[id]);
    window.draw(match.explosionRenderer); // fire on top of everything

    // Once the match is over (the engine is frozen), the last death
    // animation plays, then the end screen shows the winner for
    // GameOverRenderer::Duration seconds before leaving.
    if (!match.referee.isOver())
        return false;
    match.overFor += deltaSeconds;
    if (!match.gameOver.isStarted() && match.overFor >= CharacterRenderer::DeathDuration)
        showGameOver(match);
    match.gameOver.update(deltaSeconds);
    if (match.gameOver.isStarted())
    {
        window.setView(GameOverRenderer::viewFor(screenSize));
        window.draw(match.gameOver);
        window.setView(centeredView(match, screenSize));
    }
    return match.gameOver.isFinished();
}

} // namespace Bomberman

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "bomb_renderer.h"
#include "bot.h"
#include "bot_profile.h"
#include "character_model.h"
#include "character_renderer.h"
#include "danger_zone_renderer.h"
#include "explosion_renderer.h"
#include "game_engine.h"
#include "game_over_renderer.h"
#include "grid.h"
#include "grid_loader.h"
#include "grid_renderer.h"
#include "keyboard_controller.h"
#include "map_theme.h"
#include "match_setup.h"
#include "match_referee.h"
#include "match_setup_renderer.h"
#include "upgrade_renderer.h"

namespace
{

constexpr int LevelCount = 4;

static_assert(Bomberman::MatchSetup::DifficultyCount == Bomberman::BotDifficultyCount,
              "the menu offers one difficulty per BotDifficulty");

// $BOMBERMAN_ROOT/data/grid/level_0N.json (or ./data when BOMBERMAN_ROOT is
// not defined), N in [1, LevelCount].
std::filesystem::path levelPath(int index)
{
    const char* bombermanRoot = std::getenv("BOMBERMAN_ROOT");
    return std::filesystem::path(bombermanRoot ? bombermanRoot : ".") / "data" / "grid"
         / ("level_0" + std::to_string(index) + ".json");
}

// A map the menu offers.
struct MenuMap
{
    std::filesystem::path path;
    Bomberman::Grid grid;
};

// The level given on the command line (if any) first, then the shipped
// ones. Levels that cannot be played are reported and left out.
std::vector<MenuMap> loadMenuMaps(const std::optional<std::filesystem::path>& extra)
{
    std::vector<std::filesystem::path> paths;
    if (extra)
        paths.push_back(*extra);
    for (int i = 1; i <= LevelCount; ++i)
        paths.push_back(levelPath(i));

    std::vector<MenuMap> maps;
    for (const std::filesystem::path& path : paths)
    {
        try
        {
            Bomberman::Grid grid = Bomberman::GridLoader::loadFromFile(path);
            if (grid.getSpawnPoints().empty())
            {
                std::cerr << path << " has no spawn point" << std::endl;
                continue;
            }
            std::cout << "Loaded " << path << " (" << grid.getWidth() << "x" << grid.getHeight() << ", theme \""
                      << grid.getTheme() << "\", " << grid.getSpawnPoints().size() << " spawn points)" << std::endl;
            maps.push_back(MenuMap{path, std::move(grid)});
        }
        catch (const Bomberman::GridLoadError& error)
        {
            std::cerr << "Failed to load grid: " << error.what() << std::endl;
        }
    }
    return maps;
}

// Everything that belongs to one round; rebuilt from scratch for each
// match. The engine runs the game logic at a fixed tick rate (the bots
// think inside it, see Bot), the renderers only display its state.
struct Match
{
    Bomberman::GameEngine engine;
    Bomberman::PlayerId player; // the keyboard-controlled character
    Bomberman::GridRenderer gridRenderer;
    Bomberman::DangerZoneRenderer dangerZoneRenderer;
    Bomberman::UpgradeRenderer upgradeRenderer;
    std::vector<Bomberman::CharacterRenderer> characterRenderers; // indexed by PlayerId
    std::vector<Bomberman::CharacterModel> models;                 // indexed by PlayerId
    Bomberman::ExplosionRenderer explosionRenderer;
    Bomberman::MatchReferee referee;
    Bomberman::GameOverRenderer gameOver;
    float overFor = 0.f; // real seconds since the match was over
};

std::uint64_t freshSeed()
{
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ static_cast<std::uint64_t>(device());
}

// The keyboard player on the first spawn point, then one bot per opponent
// on the next ones (as many as the map has room for).
std::unique_ptr<Match> startMatch(const Bomberman::Grid& grid, const Bomberman::MatchSetup& setup,
                                  sf::Vector2u screenSize)
{
    // The tile size grows with the screen so the grid fills as much of it as
    // possible while keeping square tiles.
    const float tileSize = std::min(static_cast<float>(screenSize.x) / static_cast<float>(grid.getWidth()),
                                    static_cast<float>(screenSize.y) / static_cast<float>(grid.getHeight()));

    Bomberman::GameRules rules;
    rules.seed = freshSeed();
    Bomberman::GameEngine engine(grid, rules);
    const Bomberman::PlayerId player = *engine.addPlayer(); // the menu only offers maps with a spawn point
    std::vector<Bomberman::CharacterRenderer> characterRenderers = {
        Bomberman::CharacterRenderer(tileSize, Bomberman::CharacterRenderer::Palette{}, setup.pirate)};
    std::vector<Bomberman::CharacterModel> models = {setup.pirate};

    const Bomberman::BotDifficulty difficulty =
        Bomberman::AllBotDifficulties[static_cast<std::size_t>(setup.difficulty)];
    for (const Bomberman::CharacterModel model : Bomberman::opponentModels(setup.pirate, setup.opponents))
    {
        const std::optional<Bomberman::PlayerId> id = engine.addPlayer();
        if (!id)
            break; // no spawn point left
        engine.setController(*id, std::make_unique<Bomberman::Bot>(
                                      difficulty, rules.seed ^ (0x9E3779B97F4A7C15ull * static_cast<std::uint64_t>(*id))));
        characterRenderers.emplace_back(tileSize, Bomberman::CharacterRenderer::paletteForPlayer(*id), model);
        models.push_back(model);
    }

    const Bomberman::MapTheme theme = Bomberman::mapThemeFromName(grid.getTheme());
    auto match = std::make_unique<Match>(Match{std::move(engine), player, Bomberman::GridRenderer(tileSize, theme),
                                               Bomberman::DangerZoneRenderer(tileSize),
                                               Bomberman::UpgradeRenderer(tileSize), std::move(characterRenderers),
                                               std::move(models), Bomberman::ExplosionRenderer(tileSize),
                                               Bomberman::MatchReferee(player), Bomberman::GameOverRenderer{}, 0.f});
    match->gridRenderer.update(match->engine.getGrid());
    return match;
}

// The grid rarely fills the screen exactly (its aspect ratio differs from the
// screen's), so center it instead of leaving it stuck in a corner.
sf::View centeredView(const Match& match, sf::Vector2u screenSize)
{
    const sf::Vector2f screen(static_cast<float>(screenSize.x), static_cast<float>(screenSize.y));
    const sf::Vector2f board(static_cast<float>(match.gridRenderer.getPixelSize().x),
                             static_cast<float>(match.gridRenderer.getPixelSize().y));
    return sf::View(sf::FloatRect((board - screen) / 2.f, screen));
}

// Shows the end screen for the referee's verdict.
void showGameOver(Match& match)
{
    const std::optional<Bomberman::PlayerId> winner = match.referee.getWinner();
    const Bomberman::MatchOutcome outcome = !winner                ? Bomberman::MatchOutcome::Draw
                                          : *winner == match.player ? Bomberman::MatchOutcome::Victory
                                                                    : Bomberman::MatchOutcome::Defeat;
    if (winner)
        match.gameOver.start(outcome, match.models[static_cast<std::size_t>(*winner)],
                             *winner == match.player ? Bomberman::CharacterRenderer::Palette{}
                                                     : Bomberman::CharacterRenderer::paletteForPlayer(*winner));
    else
        match.gameOver.start(outcome);
}

} // namespace

int main(int argc, char* argv[])
{
    std::cout << "Start Bomberman" << std::endl;
    if (!std::getenv("BOMBERMAN_ROOT"))
        std::cout << "BOMBERMAN_ROOT is not defined, looking for data in the current directory\n";

    // Fullscreen at the desktop resolution.
    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();

    // Fail early if there is nothing to play on.
    const std::vector<MenuMap> maps =
        loadMenuMaps(argc > 1 ? std::optional<std::filesystem::path>(argv[1]) : std::nullopt);
    if (maps.empty())
        return EXIT_FAILURE;

    std::vector<Bomberman::Grid> menuGrids;
    for (const MenuMap& map : maps)
        menuGrids.push_back(map.grid);

    // Launch on the setup menu: Z/S or the up/down arrows pick a row, Q/D
    // or the left/right arrows change it, Enter/Space starts the match; Tab
    // in game comes back to it.
    Bomberman::MatchSetup setup;
    setup.mapCount = static_cast<int>(maps.size());
    Bomberman::MatchSetupRenderer menu(menuGrids);
    std::unique_ptr<Match> match;
    // The key that started the match (Space also drops bombs) must be
    // released before the keyboard drives the player.
    bool waitForRelease = false;

    const Bomberman::KeyboardController keyboard;
    std::vector<Bomberman::BombRenderer> bombRenderers;

    sf::ContextSettings windowSettings;
    windowSettings.antiAliasingLevel = 8; // smooths the character's rounded/angled shapes

    sf::RenderWindow window(desktopMode, "Bomberman", sf::Style::None, sf::State::Fullscreen, windowSettings);
    window.setFramerateLimit(60);
    window.setView(Bomberman::MatchSetupRenderer::viewFor(desktopMode.size));

    constexpr std::array<sf::Keyboard::Key, LevelCount> levelKeys = {sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2,
                                                                      sf::Keyboard::Key::Num3, sf::Keyboard::Key::Num4};

    auto play = [&]() {
        match = startMatch(maps[static_cast<std::size_t>(setup.map)].grid, setup, desktopMode.size);
        waitForRelease = true;
        bombRenderers.clear();
        window.setView(centeredView(*match, desktopMode.size));
    };
    auto backToMenu = [&]() {
        match.reset();
        window.setView(Bomberman::MatchSetupRenderer::viewFor(desktopMode.size));
    };

    sf::Clock clock;
    while (window.isOpen())
    {
        const float deltaSeconds = clock.restart().asSeconds();

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                using Key = sf::Keyboard::Key;
                const Key key = keyPressed->code;
                if (key == Key::Escape)
                    window.close();

                if (!match)
                {
                    if (key == Key::Z || key == Key::Up)
                        setup.moveFocus(-1);
                    else if (key == Key::S || key == Key::Down)
                        setup.moveFocus(1);
                    else if (key == Key::Q || key == Key::Left)
                        setup.change(-1);
                    else if (key == Key::D || key == Key::Right)
                        setup.change(1);
                    else if ((key == Key::Enter || key == Key::Space) && setup.canStart())
                        play();
                    continue;
                }

                // Tab: back to the menu.
                if (key == Key::Tab)
                {
                    backToMenu();
                    continue;
                }

                // 1-4: restart on the matching shipped map, same setup.
                const auto levelKey = std::find(levelKeys.begin(), levelKeys.end(), key);
                if (levelKey != levelKeys.end())
                {
                    const std::filesystem::path wanted =
                        levelPath(static_cast<int>(levelKey - levelKeys.begin()) + 1);
                    const auto map = std::find_if(maps.begin(), maps.end(),
                                                  [&](const MenuMap& candidate) { return candidate.path == wanted; });
                    if (map != maps.end())
                    {
                        setup.map = static_cast<int>(map - maps.begin());
                        play();
                    }
                }
            }
        }

        if (!match)
        {
            menu.update(setup, deltaSeconds);
            window.clear(sf::Color::Black);
            window.draw(menu);
            window.display();
            continue;
        }

        Bomberman::GameEngine& engine = match->engine;
        Bomberman::GridRenderer& gridRenderer = match->gridRenderer;

        // Input only produces a command; the engine decides at its next tick
        // whether the player is ready to execute it (actions are paced by
        // GameRules delays). The bots are asked by the engine itself, at
        // every tick where they are ready.
        if (waitForRelease && !keyboard.poll())
            waitForRelease = false;
        // A move is immediate (holding the key keeps walking); Space drops a
        // bomb where the player stands.
        if (const auto command = waitForRelease ? std::nullopt : keyboard.poll())
            engine.submitCommand(match->player, *command);

        // The referee speeds the match up once the player is dead (the
        // opponents fight it out) and stops it once it is over.
        engine.advance(match->referee.gameSeconds(deltaSeconds));

        for (const Bomberman::GameEvent& event : engine.takeEvents())
        {
            switch (event.type)
            {
            case Bomberman::GameEvent::Type::BombPlaced:
                match->characterRenderers[static_cast<std::size_t>(event.playerId)].triggerBombAnimation();
                break;
            case Bomberman::GameEvent::Type::PlayerKilled:
                match->characterRenderers[static_cast<std::size_t>(event.playerId)].triggerDeathAnimation();
                break;
            case Bomberman::GameEvent::Type::Explosion:
                gridRenderer.update(engine.getGrid()); // blocks were destroyed
                match->explosionRenderer.add(event.blast); // fireballs over the blast's cells
                break;
            default:
                break;
            }
        }

        // Upgrades revealed by destroyed crates, picked up by walking on them.
        match->upgradeRenderer.update(engine.getUpgrades(), deltaSeconds);
        // Cells each ticking bomb is about to hit, then the explosions themselves.
        match->dangerZoneRenderer.update(engine.getBombManager().getBombs(), engine.getGrid());
        match->explosionRenderer.update(deltaSeconds);

        for (std::size_t id = 0; id < match->characterRenderers.size(); ++id)
            match->characterRenderers[id].update(engine.getCharacter(static_cast<Bomberman::PlayerId>(id)),
                                                 deltaSeconds);

        const std::vector<Bomberman::Bomb>& bombs = engine.getBombManager().getBombs();
        bombRenderers.assign(bombs.size(), Bomberman::BombRenderer(gridRenderer.getTileSize()));
        for (std::size_t i = 0; i < bombs.size(); ++i)
            bombRenderers[i].update(bombs[i]);

        window.clear(sf::Color::Black);
        window.draw(gridRenderer);
        window.draw(match->dangerZoneRenderer);
        window.draw(match->upgradeRenderer);
        for (const Bomberman::BombRenderer& bombRenderer : bombRenderers)
            window.draw(bombRenderer);
        // The dead stay on screen while their death animation plays.
        for (std::size_t id = 0; id < match->characterRenderers.size(); ++id)
            if (engine.getCharacter(static_cast<Bomberman::PlayerId>(id)).isAlive()
                || match->characterRenderers[id].isDying())
                window.draw(match->characterRenderers[id]);
        window.draw(match->explosionRenderer); // fire on top of everything

        // Once the match is over (the engine is frozen), the last death
        // animation plays, then the end screen shows the winner for
        // GameOverRenderer::Duration seconds before the menu comes back.
        match->referee.update(engine, deltaSeconds);
        if (match->referee.isOver())
        {
            match->overFor += deltaSeconds;
            if (!match->gameOver.isStarted() && match->overFor >= Bomberman::CharacterRenderer::DeathDuration)
                showGameOver(*match);
            match->gameOver.update(deltaSeconds);
            if (match->gameOver.isStarted())
            {
                window.setView(Bomberman::GameOverRenderer::viewFor(desktopMode.size));
                window.draw(match->gameOver);
                window.setView(centeredView(*match, desktopMode.size));
            }
        }
        window.display();

        if (match->gameOver.isFinished())
            backToMenu();
    }

    return EXIT_SUCCESS;
}

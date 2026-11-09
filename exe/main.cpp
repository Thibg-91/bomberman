#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "bomb_renderer.h"
#include "character_renderer.h"
#include "game_engine.h"
#include "grid.h"
#include "grid_loader.h"
#include "grid_renderer.h"
#include "keyboard_controller.h"
#include "map_theme.h"

namespace
{

constexpr int LevelCount = 4;

// $BOMBERMAN_ROOT/data/grid/level_0N.json (or ./data when BOMBERMAN_ROOT is
// not defined), N in [1, LevelCount].
std::filesystem::path levelPath(int index)
{
    const char* bombermanRoot = std::getenv("BOMBERMAN_ROOT");
    return std::filesystem::path(bombermanRoot ? bombermanRoot : ".") / "data" / "grid"
         / ("level_0" + std::to_string(index) + ".json");
}

// Everything that belongs to one round on one map; rebuilt from scratch when
// another map is selected. The engine runs the game logic at a fixed tick
// rate, the renderers only display its state.
struct Match
{
    Bomberman::GameEngine engine;
    Bomberman::PlayerId player; // the keyboard-controlled character
    Bomberman::GridRenderer gridRenderer;
    Bomberman::CharacterRenderer playerRenderer;
};

// Nullptr (and an error on stderr) when the level cannot be used.
std::unique_ptr<Match> loadMatch(const std::filesystem::path& gridPath, sf::Vector2u screenSize)
{
    Bomberman::Grid grid;
    try
    {
        grid = Bomberman::GridLoader::loadFromFile(gridPath);
    }
    catch (const Bomberman::GridLoadError& error)
    {
        std::cerr << "Failed to load grid: " << error.what() << std::endl;
        return nullptr;
    }

    if (grid.getSpawnPoints().empty())
    {
        std::cerr << gridPath << " has no spawn point" << std::endl;
        return nullptr;
    }

    std::cout << "Loaded " << gridPath << " (" << grid.getWidth() << "x" << grid.getHeight() << ", theme \""
              << grid.getTheme() << "\", " << grid.getSpawnPoints().size() << " spawn points)" << std::endl;

    // The tile size grows with the screen so the grid fills as much of it as
    // possible while keeping square tiles.
    const float tileSize = std::min(static_cast<float>(screenSize.x) / static_cast<float>(grid.getWidth()),
                                    static_cast<float>(screenSize.y) / static_cast<float>(grid.getHeight()));

    const Bomberman::MapTheme theme = Bomberman::mapThemeFromName(grid.getTheme());
    Bomberman::GameEngine engine(std::move(grid));
    // Spawns on the first spawn point (checked above, so this cannot fail).
    const Bomberman::PlayerId player = *engine.addPlayer();

    auto match = std::make_unique<Match>(
        Match{std::move(engine), player, Bomberman::GridRenderer(tileSize, theme), Bomberman::CharacterRenderer(tileSize)});
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

} // namespace

int main(int argc, char* argv[])
{
    std::cout << "Start Bomberman" << std::endl;
    if (!std::getenv("BOMBERMAN_ROOT"))
        std::cout << "BOMBERMAN_ROOT is not defined, looking for data in the current directory\n";

    // Fullscreen at the desktop resolution.
    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();

    std::unique_ptr<Match> match = loadMatch(argc > 1 ? std::filesystem::path(argv[1]) : levelPath(1), desktopMode.size);
    if (!match)
        return EXIT_FAILURE;

    const Bomberman::KeyboardController keyboard;
    std::vector<Bomberman::BombRenderer> bombRenderers;

    sf::ContextSettings windowSettings;
    windowSettings.antiAliasingLevel = 8; // smooths the character's rounded/angled shapes

    sf::RenderWindow window(desktopMode, "Bomberman", sf::Style::None, sf::State::Fullscreen, windowSettings);
    window.setFramerateLimit(60);
    window.setView(centeredView(*match, desktopMode.size));

    constexpr std::array<sf::Keyboard::Key, LevelCount> levelKeys = {sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2,
                                                                      sf::Keyboard::Key::Num3, sf::Keyboard::Key::Num4};

    // Direction the player has just turned to with the key still held: a turn
    // consumes the key press, so walking that way needs a new press (then
    // holding it keeps walking).
    std::optional<Bomberman::Direction> turnKeyHeld;

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
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                    window.close();

                // 1-4: restart on the matching map.
                const auto levelKey = std::find(levelKeys.begin(), levelKeys.end(), keyPressed->code);
                if (levelKey != levelKeys.end())
                {
                    const int level = static_cast<int>(levelKey - levelKeys.begin()) + 1;
                    if (auto next = loadMatch(levelPath(level), desktopMode.size))
                    {
                        match = std::move(next);
                        turnKeyHeld.reset();
                        window.setView(centeredView(*match, desktopMode.size));
                    }
                }
            }
        }

        Bomberman::GameEngine& engine = match->engine;
        Bomberman::GridRenderer& gridRenderer = match->gridRenderer;
        Bomberman::CharacterRenderer& playerRenderer = match->playerRenderer;

        if (turnKeyHeld && !keyboard.isHeld(*turnKeyHeld))
            turnKeyHeld.reset();

        // Input only produces a command; the engine decides at its next tick
        // whether the player is ready to execute it (actions are paced by
        // GameRules delays). An AI or a remote player would submit its own
        // commands the same way with its own id.
        if (const auto command = keyboard.poll())
        {
            const bool consumedTurn =
                command->type == Bomberman::CharacterCommand::Type::Move && turnKeyHeld == command->direction;
            if (!consumedTurn)
                engine.submitCommand(match->player, *command);
        }

        engine.advance(deltaSeconds);

        for (const Bomberman::GameEvent& event : engine.takeEvents())
        {
            switch (event.type)
            {
            case Bomberman::GameEvent::Type::PlayerTurned:
                if (event.playerId == match->player)
                    turnKeyHeld = engine.getCharacter(match->player).getFacing();
                break;
            case Bomberman::GameEvent::Type::BombPlaced:
                if (event.playerId == match->player)
                    playerRenderer.triggerBombAnimation();
                break;
            case Bomberman::GameEvent::Type::Explosion:
                gridRenderer.update(engine.getGrid()); // blocks were destroyed
                break;
            default:
                break;
            }
        }

        const Bomberman::Character& player = engine.getCharacter(match->player);
        playerRenderer.update(player, deltaSeconds);

        const std::vector<Bomberman::Bomb>& bombs = engine.getBombManager().getBombs();
        bombRenderers.assign(bombs.size(), Bomberman::BombRenderer(gridRenderer.getTileSize()));
        for (std::size_t i = 0; i < bombs.size(); ++i)
            bombRenderers[i].update(bombs[i]);

        window.clear(sf::Color::Black);
        window.draw(gridRenderer);
        for (const Bomberman::BombRenderer& bombRenderer : bombRenderers)
            window.draw(bombRenderer);
        if (player.isAlive())
            window.draw(playerRenderer);
        window.display();
    }

    return EXIT_SUCCESS;
}

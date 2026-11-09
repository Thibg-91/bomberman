#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "bomb_manager.h"
#include "bomb_renderer.h"
#include "character.h"
#include "character_renderer.h"
#include "grid.h"
#include "grid_loader.h"
#include "grid_renderer.h"
#include "keyboard_controller.h"
#include "map_theme.h"

namespace
{

constexpr int LevelCount = 4;
constexpr int PlayerId = 0; // owner id of the player's bombs

// $BOMBERMAN_ROOT/data/grid/level_0N.json (or ./data when BOMBERMAN_ROOT is
// not defined), N in [1, LevelCount].
std::filesystem::path levelPath(int index)
{
    const char* bombermanRoot = std::getenv("BOMBERMAN_ROOT");
    return std::filesystem::path(bombermanRoot ? bombermanRoot : ".") / "data" / "grid"
         / ("level_0" + std::to_string(index) + ".json");
}

// Everything that belongs to one round on one map; rebuilt from scratch when
// another map is selected.
struct Match
{
    Bomberman::Grid grid;
    Bomberman::GridRenderer gridRenderer;
    Bomberman::Character player;
    Bomberman::CharacterRenderer playerRenderer;
    Bomberman::BombManager bombManager;
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

    const Bomberman::Position spawn = grid.getSpawnPoints().front();
    auto match = std::make_unique<Match>(Match{grid,
                                               Bomberman::GridRenderer(tileSize, Bomberman::mapThemeFromName(grid.getTheme())),
                                               Bomberman::Character(spawn), Bomberman::CharacterRenderer(tileSize),
                                               Bomberman::BombManager()});
    match->gridRenderer.update(match->grid);
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

        Bomberman::Grid& grid = match->grid;
        Bomberman::Character& player = match->player;
        Bomberman::BombManager& bombManager = match->bombManager;
        Bomberman::GridRenderer& gridRenderer = match->gridRenderer;
        Bomberman::CharacterRenderer& playerRenderer = match->playerRenderer;

        player.update(deltaSeconds);

        if (turnKeyHeld && !keyboard.isHeld(*turnKeyHeld))
            turnKeyHeld.reset();

        if (player.isAlive())
        {
            if (const auto command = keyboard.poll())
            {
                const Bomberman::Position facedCell = Bomberman::step(player.getPosition(), player.getFacing());

                if (command->type == Bomberman::CharacterCommand::Type::PlaceBomb)
                {
                    // One active bomb per character for now (see
                    // BombManager::DefaultMaxActiveBombs).
                    if (bombManager.placeBomb(facedCell, grid, PlayerId))
                        playerRenderer.triggerBombAnimation();
                }
                else if (command->type == Bomberman::CharacterCommand::Type::PushBomb)
                {
                    // Does nothing when there is no bomb in front of the
                    // character: blocks are only destroyed by explosions.
                    bombManager.pushBombAt(facedCell, player.getFacing(), grid);
                }
                else
                {
                    if (turnKeyHeld != command->direction)
                    {
                        const Bomberman::Direction facingBefore = player.getFacing();
                        // Bombs block the way like blocks do.
                        player.execute(*command, grid, bombManager.getBombPositions());
                        if (player.getFacing() != facingBefore)
                            turnKeyHeld = command->direction;
                    }
                }
            }
        }

        const std::vector<Bomberman::Position> explodedCells = bombManager.update(deltaSeconds, grid);
        if (!explodedCells.empty())
        {
            gridRenderer.update(grid);
            if (std::find(explodedCells.begin(), explodedCells.end(), player.getPosition()) != explodedCells.end())
                player.kill();
        }

        playerRenderer.update(player, deltaSeconds);

        bombRenderers.assign(bombManager.getBombs().size(), Bomberman::BombRenderer(gridRenderer.getTileSize()));
        for (std::size_t i = 0; i < bombManager.getBombs().size(); ++i)
            bombRenderers[i].update(bombManager.getBombs()[i]);

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

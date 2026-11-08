#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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

namespace
{

// Level given on the command line, otherwise the default level shipped in
// $BOMBERMAN_ROOT/data (or ./data when BOMBERMAN_ROOT is not defined).
std::filesystem::path resolveGridPath(int argc, char* argv[])
{
    if (argc > 1)
        return argv[1];

    const char* bombermanRoot = std::getenv("BOMBERMAN_ROOT");
    if (!bombermanRoot)
        std::cout << "BOMBERMAN_ROOT is not defined, looking for data in the current directory\n";

    return std::filesystem::path(bombermanRoot ? bombermanRoot : ".") / "data" / "grid" / "level_01.json";
}

} // namespace

int main(int argc, char* argv[])
{
    std::cout << "Start Bomberman" << std::endl;

    const std::filesystem::path gridPath = resolveGridPath(argc, argv);

    Bomberman::Grid grid;
    try
    {
        grid = Bomberman::GridLoader::loadFromFile(gridPath);
    }
    catch (const Bomberman::GridLoadError& error)
    {
        std::cerr << "Failed to load grid: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Loaded " << gridPath << " (" << grid.getWidth() << "x" << grid.getHeight() << ", "
              << grid.getSpawnPoints().size() << " spawn points)" << std::endl;

    Bomberman::GridRenderer gridRenderer;
    gridRenderer.update(grid);

    if (grid.getSpawnPoints().empty())
    {
        std::cerr << "The grid has no spawn point" << std::endl;
        return EXIT_FAILURE;
    }

    Bomberman::Character player(grid.getSpawnPoints().front());
    Bomberman::CharacterRenderer playerRenderer(gridRenderer.getTileSize());
    const Bomberman::KeyboardController keyboard;

    Bomberman::BombManager bombManager;
    std::vector<Bomberman::BombRenderer> bombRenderers;

    sf::RenderWindow window(sf::VideoMode(gridRenderer.getPixelSize()), "Bomberman", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Clock clock;
    while (window.isOpen())
    {
        const float deltaSeconds = clock.restart().asSeconds();
        player.update(deltaSeconds);

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                    window.close();
            }
        }

        if (player.isAlive())
        {
            if (const auto command = keyboard.poll())
            {
                const Bomberman::Position facedCell = Bomberman::step(player.getPosition(), player.getFacing());

                if (command->type == Bomberman::CharacterCommand::Type::PlaceBomb)
                {
                    bombManager.placeBomb(facedCell, grid);
                }
                else if (command->type == Bomberman::CharacterCommand::Type::BreakBlock
                         && bombManager.hasBombAt(facedCell))
                {
                    bombManager.pushBombAt(facedCell, player.getFacing(), grid);
                }
                else
                {
                    // The back (grid) changed: keep the front (renderer) in sync.
                    const bool breaksBlock = command->type == Bomberman::CharacterCommand::Type::BreakBlock;
                    if (player.execute(*command, grid) && breaksBlock)
                        gridRenderer.update(grid);
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

        playerRenderer.update(player);

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

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <SFML/Graphics.hpp>

#include "grid.h"
#include "grid_loader.h"
#include "grid_renderer.h"

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

    sf::RenderWindow window(sf::VideoMode(gridRenderer.getPixelSize()), "Bomberman", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    while (window.isOpen())
    {
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

        window.clear(sf::Color::Black);
        window.draw(gridRenderer);
        window.display();
    }

    return EXIT_SUCCESS;
}

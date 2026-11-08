#include "grid_loader.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace Bomberman
{

namespace
{

int readDimension(const nlohmann::json& document, const char* key)
{
    const auto it = document.find(key);
    if (it == document.end() || !it->is_number_integer())
        throw GridLoadError(std::string("Missing or non-integer \"") + key + "\" field");

    const int value = it->get<int>();
    if (value <= 0)
        throw GridLoadError(std::string("\"") + key + "\" must be strictly positive");

    return value;
}

std::string at(int x, int y)
{
    return " at (" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

} // namespace

// =========================
// Static Methods
// =========================
Grid GridLoader::loadFromFile(const std::filesystem::path& gridFile)
{
    std::ifstream stream(gridFile);
    if (!stream)
        throw GridLoadError("Cannot open grid file: " + gridFile.string());

    std::ostringstream content;
    content << stream.rdbuf();

    try
    {
        return loadFromString(content.str());
    }
    catch (const GridLoadError& error)
    {
        throw GridLoadError(gridFile.string() + ": " + error.what());
    }
}

Grid GridLoader::loadFromString(const std::string& jsonContent)
{
    nlohmann::json document;
    try
    {
        document = nlohmann::json::parse(jsonContent);
    }
    catch (const nlohmann::json::parse_error& error)
    {
        throw GridLoadError(std::string("Invalid JSON: ") + error.what());
    }

    if (!document.is_object())
        throw GridLoadError("Grid description must be a JSON object");

    const int width = readDimension(document, "width");
    const int height = readDimension(document, "height");

    const auto rows = document.find("rows");
    if (rows == document.end() || !rows->is_array())
        throw GridLoadError("Missing or non-array \"rows\" field");
    if (rows->size() != static_cast<std::size_t>(height))
        throw GridLoadError("Expected " + std::to_string(height) + " rows, got "
                            + std::to_string(rows->size()));

    Grid grid(width, height);
    for (int y = 0; y < height; ++y)
    {
        const auto& row = (*rows)[static_cast<std::size_t>(y)];
        if (!row.is_string())
            throw GridLoadError("Row " + std::to_string(y) + " is not a string");

        const auto& line = row.get_ref<const std::string&>();
        if (line.size() != static_cast<std::size_t>(width))
            throw GridLoadError("Row " + std::to_string(y) + " has " + std::to_string(line.size())
                                + " cells, expected " + std::to_string(width));

        for (int x = 0; x < width; ++x)
        {
            const char symbol = line[static_cast<std::size_t>(x)];
            switch (symbol)
            {
            case WallSymbol:
                grid.setTile({x, y}, Tile(TileType::Wall));
                break;
            case DestructibleBlockSymbol:
                grid.setTile({x, y}, Tile(TileType::DestructibleBlock));
                break;
            case EmptySymbol:
                break;
            case SpawnSymbol:
                grid.addSpawnPoint({x, y});
                break;
            default:
                throw GridLoadError(std::string("Unknown symbol '") + symbol + "'" + at(x, y));
            }
        }
    }

    return grid;
}

} // namespace Bomberman

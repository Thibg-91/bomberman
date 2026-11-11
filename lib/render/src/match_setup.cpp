#include "match_setup.h"

#include <algorithm>
#include <cstddef>

namespace Bomberman
{

namespace
{

int indexOf(CharacterModel model)
{
    const auto it = std::find(AllCharacterModels.begin(), AllCharacterModels.end(), model);
    return static_cast<int>(it - AllCharacterModels.begin());
}

} // namespace

void MatchSetup::moveFocus(int step) noexcept
{
    if (step == 0)
        return;
    const int direction = step > 0 ? 1 : -1;
    for (int moved = 0; moved < (step > 0 ? step : -step); ++moved)
    {
        int row = static_cast<int>(focus);
        for (int k = 0; k < RowCount; ++k) // Mode is always active: this ends
        {
            row = wrap(row, direction, RowCount);
            if (isRowActive(static_cast<Row>(row)))
                break;
        }
        focus = static_cast<Row>(row);
    }
}

bool MatchSetup::isRowActive(Row row) const noexcept
{
    switch (row)
    {
    case Row::Mode:
    case Row::Pirate:
        return true;
    case Row::Map:
        return mode != GameMode::Join;
    case Row::Difficulty:
    case Row::Opponents:
        break;
    }
    return mode == GameMode::VersusComputer;
}

void MatchSetup::change(int step) noexcept
{
    switch (focus)
    {
    case Row::Mode:
        mode = AllGameModes[static_cast<std::size_t>(wrap(static_cast<int>(mode), step, GameModeCount))];
        break;
    case Row::Pirate:
        pirate = AllCharacterModels[static_cast<std::size_t>(wrap(indexOf(pirate), step, CharacterModelCount))];
        break;
    case Row::Map:
        map = mapCount > 0 ? wrap(map, step, mapCount) : 0;
        break;
    case Row::Difficulty:
        difficulty = wrap(difficulty, step, DifficultyCount);
        break;
    case Row::Opponents:
        opponents = wrap(opponents - 1, step, MaxOpponents) + 1;
        break;
    }
}

bool MatchSetup::canStart() const noexcept
{
    return mode == GameMode::Join || (map >= 0 && map < mapCount);
}

int MatchSetup::wrap(int value, int step, int count) noexcept
{
    if (count <= 0)
        return 0;
    return ((value + step) % count + count) % count;
}

std::vector<CharacterModel> opponentModels(CharacterModel player, int count)
{
    std::vector<CharacterModel> models;
    for (const CharacterModel model : AllCharacterModels)
        if (model != player && static_cast<int>(models.size()) < count)
            models.push_back(model);
    return models;
}

} // namespace Bomberman

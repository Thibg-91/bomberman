#ifndef CHARACTER_MODEL_H
#define CHARACTER_MODEL_H

#pragma once

#include <array>

namespace Bomberman
{

// The look of a character (purely visual: every model plays exactly the
// same). They all share the same template (round body, arms, legs, same
// mock-up coordinates), so walking, hopping and the bomb pose work alike;
// only the head, the face and the colors differ.
enum class CharacterModel
{
    Buccaneer, // knotted polka-dot bandana, one eye, eyepatch and scar (the original)
    Captain,   // gold-trimmed tricorne with a skull, handlebar mustache
    Lookout,   // striped beanie with a pompom, freckles
    Parrot,    // green feathers, hooked beak, red crest, wings and orange feet
    Revenant   // skeleton with glowing eye sockets and a dark bandana
};

inline constexpr int CharacterModelCount = 5;
inline constexpr std::array<CharacterModel, CharacterModelCount> AllCharacterModels = {
    CharacterModel::Buccaneer, CharacterModel::Captain, CharacterModel::Lookout, CharacterModel::Parrot,
    CharacterModel::Revenant};

} // namespace Bomberman

#endif // CHARACTER_MODEL_H

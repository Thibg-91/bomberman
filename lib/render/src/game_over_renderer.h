#ifndef GAME_OVER_RENDERER_H
#define GAME_OVER_RENDERER_H

#pragma once

#include <optional>

#include <SFML/Graphics.hpp>

#include "character_model.h"
#include "character_renderer.h"

namespace Bomberman
{

// How the match ended, from the keyboard player's point of view.
enum class MatchOutcome
{
    Victory, // the keyboard player is the last one standing
    Defeat,  // an opponent is
    Draw     // nobody: the last ones died together (or time ran out)
};

// The end screen (validated mock-up "Fin de l'abordage", screen "Le pavillon
// noir", chest "Le coffre débordant"), shown over the frozen board for
// Duration seconds:
//   - the board darkens;
//   - a black Jolly Roger flag drops from the top and waves, crossed bones
//     on it, the letters of GAME OVER falling onto it one by one (gold for
//     a victory, bone white otherwise);
//   - under a spotlight, an open treasure chest overflowing with coins and
//     gems pops up, the winner standing in the gold with a crown (a skull
//     instead for a draw), hopping;
//   - for a victory, gold coins rain down.
// The letters are drawn with round strokes (no font in the project).
// Everything is laid out in a DesignSize (1600x900) space, like the menu.
class GameOverRenderer : public sf::Drawable
{
public:
    static constexpr sf::Vector2f DesignSize{1600.f, 900.f};
    static constexpr float Duration = 3.f;      // seconds on screen
    static constexpr float WinnerSize = 230.f;  // design units (mock-up view box drawn this big)
    static constexpr int LetterCount = 8;       // G A M E O V E R

    // =========================
    // Static Methods
    // =========================
    [[nodiscard]] static sf::View viewFor(sf::Vector2u screenSize);

    // =========================
    // Public Methods
    // =========================
    // Starts the screen from its first frame. winner: the model (and
    // palette) of the last pirate standing; ignored for a draw.
    void start(MatchOutcome outcome, std::optional<CharacterModel> winner = std::nullopt,
               CharacterRenderer::Palette winnerPalette = CharacterRenderer::Palette{});

    // Advances the animation and rebuilds the screen.
    void update(float deltaSeconds);

    [[nodiscard]] bool isStarted() const noexcept;
    // Duration has elapsed: time to go back to the menu.
    [[nodiscard]] bool isFinished() const noexcept;
    [[nodiscard]] float getElapsed() const noexcept;
    [[nodiscard]] MatchOutcome getOutcome() const noexcept;

    // Darkening, flag, lettering, spotlight and the chest's back (under the
    // winner).
    [[nodiscard]] const sf::VertexArray& getBack() const noexcept;
    // Crown, the chest's front, the skull of a draw, falling coins (over the
    // winner).
    [[nodiscard]] const sf::VertexArray& getFront() const noexcept;
    // The winner, once it has appeared (never for a draw).
    [[nodiscard]] const std::optional<CharacterRenderer>& getWinner() const noexcept;
    // Center of letter i of GAME OVER (design units), drop included.
    [[nodiscard]] sf::Vector2f getLetterCenter(int i) const noexcept;

private:
    // =========================
    // Private Methods
    // =========================
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
    void rebuild();

    // =========================
    // Members
    // =========================
    bool m_started = false;
    float m_elapsed = 0.f;
    MatchOutcome m_outcome = MatchOutcome::Draw;
    std::optional<CharacterModel> m_winnerModel;
    CharacterRenderer::Palette m_winnerPalette;
    sf::VertexArray m_back{sf::PrimitiveType::Triangles};
    sf::VertexArray m_front{sf::PrimitiveType::Triangles};
    std::optional<CharacterRenderer> m_winner;
    sf::Vector2f m_letters[LetterCount];
};

} // namespace Bomberman

#endif // GAME_OVER_RENDERER_H

#ifndef BOMB_H
#define BOMB_H

#pragma once

#include "position.h"

namespace Bomberman
{

// A bomb dropped on the grid. Purely logical: it just knows where it is and
// how long until it explodes. BombManager owns the collection and applies the
// explosion/push effects on the grid.
class Bomb
{
public:
    static constexpr float DefaultFuseDuration = 2.f; // seconds before exploding
    static constexpr int DefaultBlastRadius = 1;       // cells per cross arm
    // A fuse with less than this left (seconds) has burnt out.
    static constexpr float TimeTolerance = 1e-4f;

    // =========================
    // Constructors / Destructor
    // =========================
    // ownerId identifies the character who dropped it (e.g. its player
    // index), used to cap how many bombs each character has active.
    explicit Bomb(Position position, float fuseDuration = DefaultFuseDuration,
                  int blastRadius = DefaultBlastRadius, int ownerId = 0) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] Position getPosition() const noexcept;
    void setPosition(Position position) noexcept;

    [[nodiscard]] int getOwnerId() const noexcept;
    [[nodiscard]] float getFuseDuration() const noexcept;
    [[nodiscard]] float getTimeRemaining() const noexcept;
    [[nodiscard]] int getBlastRadius() const noexcept;

    [[nodiscard]] bool hasExploded() const noexcept;

    // Make time pass (seconds); the bomb explodes once the fuse reaches zero.
    void update(float deltaSeconds) noexcept;

private:
    // =========================
    // Members
    // =========================
    Position m_position;
    int m_ownerId;
    float m_fuseDuration;
    float m_timeRemaining;
    int m_blastRadius;
};

} // namespace Bomberman

#endif // BOMB_H

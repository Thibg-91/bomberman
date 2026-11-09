#ifndef FIXED_TIMESTEP_H
#define FIXED_TIMESTEP_H

#pragma once

namespace Bomberman
{

// "Fixed timestep + accumulator" pattern: converts the variable real time
// elapsed between two frames into a whole number of fixed simulation ticks.
// The leftover time is kept for the next frame, so on average exactly
// tickRate ticks run per second whatever the frame rate.
class FixedTimestep
{
public:
    // After a long freeze (window dragged, debugger break...) at most this
    // many ticks are run in one go; the rest of the backlog is dropped
    // instead of making the game fast-forward (or spiral into ever longer
    // frames).
    static constexpr int DefaultMaxTicksPerFrame = 5;

    // =========================
    // Constructors / Destructor
    // =========================
    // tickRate is clamped to at least 1 tick per second, maxTicksPerFrame
    // to at least 1.
    explicit FixedTimestep(int tickRate, int maxTicksPerFrame = DefaultMaxTicksPerFrame) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] int getTickRate() const noexcept;
    [[nodiscard]] float getTickDuration() const noexcept;
    [[nodiscard]] int getMaxTicksPerFrame() const noexcept;

    // Adds realSeconds (ignored when negative) and returns how many ticks
    // must be run now.
    [[nodiscard]] int consume(float realSeconds) noexcept;

    // Fraction [0, 1) of a tick waiting in the accumulator, for renderers
    // wanting to interpolate between the last two ticks.
    [[nodiscard]] float getAlpha() const noexcept;

    void reset() noexcept;

private:
    // =========================
    // Members
    // =========================
    int m_tickRate;
    int m_maxTicksPerFrame;
    double m_tickDuration;
    double m_accumulator = 0.0; // double: no drift over long sessions
};

} // namespace Bomberman

#endif // FIXED_TIMESTEP_H

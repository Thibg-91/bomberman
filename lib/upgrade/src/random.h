#ifndef RANDOM_H
#define RANDOM_H

#pragma once

#include <cstdint>

namespace Bomberman
{

// Small deterministic pseudo-random generator (SplitMix64). Unlike the
// <random> distributions, its output is specified here and identical on every
// compiler and platform: the same seed always gives the same game, which
// replays and lock-step network play rely on.
class Random
{
public:
    static constexpr std::uint64_t DefaultSeed = 0x5EED'B0B5'1234'ABCDull;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit Random(std::uint64_t seed = DefaultSeed) noexcept;

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] std::uint64_t next() noexcept;

    // Uniform integer in [0, bound); 0 when bound <= 0.
    [[nodiscard]] int below(int bound) noexcept;

    // True with probability percent / 100 (always false at 0, always true
    // at 100 or more).
    [[nodiscard]] bool percent(int percent) noexcept;

private:
    // =========================
    // Members
    // =========================
    std::uint64_t m_state;
};

} // namespace Bomberman

#endif // RANDOM_H

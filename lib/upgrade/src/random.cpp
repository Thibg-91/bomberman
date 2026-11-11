#include "random.h"

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Random::Random(std::uint64_t seed) noexcept
    : m_state(seed)
{
}

// =========================
// Public Methods
// =========================
std::uint64_t Random::next() noexcept
{
    m_state += 0x9E3779B97F4A7C15ull;
    std::uint64_t z = m_state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

int Random::below(int bound) noexcept
{
    if (bound <= 0)
        return 0;
    // Multiply-shift on the high 32 bits: uniform enough for game odds and
    // free of the modulo bias.
    const std::uint64_t high = next() >> 32;
    return static_cast<int>((high * static_cast<std::uint64_t>(bound)) >> 32);
}

bool Random::percent(int percent) noexcept
{
    if (percent <= 0)
        return false;
    if (percent >= 100)
        return true;
    return below(100) < percent;
}

} // namespace Bomberman

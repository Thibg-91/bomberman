#include "fixed_timestep.h"

#include <algorithm>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
FixedTimestep::FixedTimestep(int tickRate, int maxTicksPerFrame) noexcept
    : m_tickRate(std::max(tickRate, 1)),
      m_maxTicksPerFrame(std::max(maxTicksPerFrame, 1)),
      m_tickDuration(1.0 / static_cast<double>(m_tickRate))
{
}

// =========================
// Public Methods
// =========================
int FixedTimestep::getTickRate() const noexcept
{
    return m_tickRate;
}

float FixedTimestep::getTickDuration() const noexcept
{
    return static_cast<float>(m_tickDuration);
}

int FixedTimestep::getMaxTicksPerFrame() const noexcept
{
    return m_maxTicksPerFrame;
}

int FixedTimestep::consume(float realSeconds) noexcept
{
    if (realSeconds > 0.f)
        m_accumulator += static_cast<double>(realSeconds);

    // A frame time is rarely an exact multiple of the tick duration: tolerate
    // a tiny rounding error so 1/60 s really gives one tick.
    constexpr double Tolerance = 1e-6;
    int ticks = 0;
    while (m_accumulator + Tolerance >= m_tickDuration)
    {
        m_accumulator = std::max(m_accumulator - m_tickDuration, 0.0);
        ++ticks;
    }

    if (ticks > m_maxTicksPerFrame)
    {
        ticks = m_maxTicksPerFrame;
        m_accumulator = 0.0; // drop the backlog
    }
    return ticks;
}

float FixedTimestep::getAlpha() const noexcept
{
    return static_cast<float>(std::clamp(m_accumulator / m_tickDuration, 0.0, 1.0));
}

void FixedTimestep::reset() noexcept
{
    m_accumulator = 0.0;
}

} // namespace Bomberman

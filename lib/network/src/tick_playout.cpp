#include "tick_playout.h"

#include <algorithm>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
TickPlayout::TickPlayout(int tickRate)
    : m_timestep(tickRate)
{
}

// =========================
// Public Methods
// =========================
void TickPlayout::push(const Ticks& ticks)
{
    m_frames.insert(m_frames.end(), ticks.frames.begin(), ticks.frames.end());
    if (ticks.checksum)
        m_checksums.push_back(*ticks.checksum);
}

std::vector<TickFrame> TickPlayout::take(float realSeconds)
{
    const int due = m_timestep.consume(realSeconds);
    const int backlog = getBacklog();
    int count = std::min(due, backlog);
    if (backlog - count > MaxBacklog)
        count = backlog - TargetBacklog; // catch up at once

    std::vector<TickFrame> frames(m_frames.begin(), m_frames.begin() + count);
    m_frames.erase(m_frames.begin(), m_frames.begin() + count);
    return frames;
}

int TickPlayout::getBacklog() const noexcept
{
    return static_cast<int>(m_frames.size());
}

std::optional<std::uint64_t> TickPlayout::takeChecksum(std::uint64_t tick)
{
    while (!m_checksums.empty() && m_checksums.front().tick < tick)
        m_checksums.pop_front();
    if (m_checksums.empty() || m_checksums.front().tick != tick)
        return std::nullopt;
    const std::uint64_t hash = m_checksums.front().hash;
    m_checksums.pop_front();
    return hash;
}

} // namespace Bomberman

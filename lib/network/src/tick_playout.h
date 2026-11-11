#ifndef TICK_PLAYOUT_H
#define TICK_PLAYOUT_H

#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "fixed_timestep.h"
#include "protocol.h"

namespace Bomberman
{

// Paces the host's ticks on a client. They arrive in bursts (one Ticks per
// host frame, delayed by a jittery Wi-Fi); running them as they come would
// make the board stutter. The playout buffers them and runs them at the
// tick rate, like the host's own FixedTimestep, and catches up when too
// many pile up (after a network hiccup) so the client never drifts far
// behind the host. It also keeps the host's checksums until the client
// engine reaches their tick.
class TickPlayout
{
public:
    // Ticks kept after catching up, and the backlog that triggers it: at
    // most MaxBacklog ticks (67 ms at 60 Hz) of extra delay.
    static constexpr int TargetBacklog = 1;
    static constexpr int MaxBacklog = 4;

    // =========================
    // Constructors / Destructor
    // =========================
    explicit TickPlayout(int tickRate = GameRules::DefaultTickRate);

    // =========================
    // Public Methods
    // =========================
    void push(const Ticks& ticks);
    // Frames to run now for a frame of realSeconds, oldest first.
    [[nodiscard]] std::vector<TickFrame> take(float realSeconds);
    [[nodiscard]] int getBacklog() const noexcept;

    // The host's hash for the state at tick (GameEngine::getTick()), if it
    // sent one; forgets it and every older one.
    [[nodiscard]] std::optional<std::uint64_t> takeChecksum(std::uint64_t tick);

private:
    // =========================
    // Members
    // =========================
    FixedTimestep m_timestep;
    std::deque<TickFrame> m_frames;
    std::deque<StateChecksum> m_checksums;
};

} // namespace Bomberman

#endif // TICK_PLAYOUT_H

#include <gtest/gtest.h>

#include "tick_playout.h"

using Bomberman::StateChecksum;
using Bomberman::TickFrame;
using Bomberman::TickPlayout;
using Bomberman::Ticks;

namespace
{

constexpr float Tick = 1.f / 60.f;

Ticks ticks(std::uint64_t first, int count)
{
    Ticks message;
    for (int i = 0; i < count; ++i)
        message.frames.push_back(TickFrame{first + static_cast<std::uint64_t>(i), {}});
    return message;
}

} // namespace

TEST(TickPlayoutTest, RunsTheReceivedTicksAtTheTickRate)
{
    TickPlayout playout;
    playout.push(ticks(0, 3));

    const auto first = playout.take(Tick);
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0].tick, 0u);
    EXPECT_EQ(playout.take(Tick).front().tick, 1u);
    EXPECT_EQ(playout.getBacklog(), 1);
}

TEST(TickPlayoutTest, NeverRunsTicksItHasNotReceived)
{
    TickPlayout playout;
    EXPECT_TRUE(playout.take(Tick).empty());
    playout.push(ticks(0, 1));
    EXPECT_EQ(playout.take(3 * Tick).size(), 1u);
}

TEST(TickPlayoutTest, SmoothsABurstOverTheNextFrames)
{
    TickPlayout playout;
    playout.push(ticks(0, 4)); // a late packet and the next ones together
    int run = 0;
    for (int frame = 0; frame < 4; ++frame)
    {
        const auto frames = playout.take(Tick);
        EXPECT_EQ(frames.size(), 1u) << "frame " << frame;
        run += static_cast<int>(frames.size());
    }
    EXPECT_EQ(run, 4);
}

TEST(TickPlayoutTest, CatchesUpWhenTooManyTicksPileUp)
{
    TickPlayout playout;
    playout.push(ticks(0, 30)); // half a second of hiccup
    const auto frames = playout.take(Tick);
    EXPECT_EQ(playout.getBacklog(), TickPlayout::TargetBacklog);
    EXPECT_EQ(frames.size(), 30u - TickPlayout::TargetBacklog);
    for (std::size_t i = 0; i < frames.size(); ++i)
        EXPECT_EQ(frames[i].tick, i); // in order
}

TEST(TickPlayoutTest, ChecksumsWaitForTheirTick)
{
    TickPlayout playout;
    Ticks first = ticks(0, 60);
    first.checksum = StateChecksum{60, 111};
    Ticks second = ticks(60, 60);
    second.checksum = StateChecksum{120, 222};
    playout.push(first);
    playout.push(second);

    EXPECT_EQ(playout.takeChecksum(59), std::nullopt);
    EXPECT_EQ(playout.takeChecksum(60), 111u);
    EXPECT_EQ(playout.takeChecksum(60), std::nullopt); // taken
    EXPECT_EQ(playout.takeChecksum(130), std::nullopt); // 120 was skipped: forgotten
}

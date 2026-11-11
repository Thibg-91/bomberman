#include <gtest/gtest.h>

#include "lobby.h"

using Bomberman::Lobby;
using Bomberman::MaxPlayers;

namespace
{

constexpr int CrewSize = 6; // CharacterModelCount

} // namespace

TEST(LobbyTest, TheHostSitsOnSeatZero)
{
    const Lobby lobby(CrewSize, 2);
    EXPECT_EQ(lobby.getSeatCount(), MaxPlayers);
    EXPECT_EQ(lobby.getModel(0), 2);
    EXPECT_EQ(lobby.countPlayers(), 1);
    EXPECT_FALSE(lobby.canStart()); // alone
    EXPECT_EQ(lobby.getTakenSeats(), std::vector<int>{0});
}

TEST(LobbyTest, NewcomersTakeTheFirstFreeSeatWithTheirModel)
{
    Lobby lobby(CrewSize, 0);
    EXPECT_EQ(lobby.join(3), 1);
    EXPECT_EQ(lobby.getModel(1), 3);
    EXPECT_TRUE(lobby.canStart());
    EXPECT_EQ(lobby.join(4), 2);

    lobby.leave(1);
    EXPECT_FALSE(lobby.isTaken(1));
    EXPECT_EQ(lobby.join(5), 1); // the freed seat first
    EXPECT_EQ(lobby.getTakenSeats(), (std::vector<int>{0, 1, 2}));
}

TEST(LobbyTest, ATakenModelIsReplacedByTheNextFreeOne)
{
    Lobby lobby(CrewSize, 1);
    ASSERT_EQ(lobby.join(1), 1);
    EXPECT_EQ(lobby.getModel(1), 2);
    ASSERT_EQ(lobby.join(1), 2);
    EXPECT_EQ(lobby.getModel(2), 3);
    ASSERT_EQ(lobby.join(5), 3);
    EXPECT_EQ(lobby.getModel(3), 5);
}

TEST(LobbyTest, TheRoomIsFullAfterFourPlayersOrTheMapSpawnPoints)
{
    Lobby lobby(CrewSize, 0);
    for (int i = 0; i < MaxPlayers - 1; ++i)
        ASSERT_TRUE(lobby.join(0).has_value());
    EXPECT_FALSE(lobby.join(0).has_value());

    Lobby smallMap(CrewSize, 0, 2);
    EXPECT_EQ(smallMap.getSeatCount(), 2);
    EXPECT_TRUE(smallMap.join(0).has_value());
    EXPECT_FALSE(smallMap.join(0).has_value());
}

TEST(LobbyTest, TheHostSeatCannotBeLeft)
{
    Lobby lobby(CrewSize, 0);
    lobby.leave(0);
    EXPECT_TRUE(lobby.isTaken(0));
}

TEST(LobbyTest, ChangingModelSkipsTheTakenOnesAndWraps)
{
    Lobby lobby(CrewSize, 0);
    ASSERT_EQ(lobby.join(1), 1);
    ASSERT_EQ(lobby.join(2), 2);

    EXPECT_TRUE(lobby.changeModel(0, 1)); // 1 and 2 are taken
    EXPECT_EQ(lobby.getModel(0), 3);
    EXPECT_TRUE(lobby.changeModel(0, -1)); // back over 2 and 1 to 0
    EXPECT_EQ(lobby.getModel(0), 0);
    EXPECT_TRUE(lobby.changeModel(0, -1)); // wraps to the end of the crew
    EXPECT_EQ(lobby.getModel(0), CrewSize - 1);
    EXPECT_FALSE(lobby.changeModel(3, 1)); // free seat
}

TEST(LobbyTest, NoChangeWhenEveryOtherModelIsTaken)
{
    Lobby lobby(2, 0, 2);
    ASSERT_EQ(lobby.join(0), 1);
    EXPECT_EQ(lobby.getModel(1), 1);
    EXPECT_FALSE(lobby.changeModel(1, 1));
    EXPECT_EQ(lobby.getModel(1), 1);
}

TEST(LobbyTest, SeatInfosDescribeTheRoom)
{
    Lobby lobby(CrewSize, 4);
    ASSERT_EQ(lobby.join(2), 1);
    const auto infos = lobby.toSeatInfos();
    EXPECT_TRUE(infos[0].taken);
    EXPECT_EQ(infos[0].model, 4);
    EXPECT_TRUE(infos[1].taken);
    EXPECT_EQ(infos[1].model, 2);
    EXPECT_FALSE(infos[2].taken);
    EXPECT_FALSE(infos[3].taken);
}

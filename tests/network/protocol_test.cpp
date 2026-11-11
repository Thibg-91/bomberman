#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "game_engine.h"
#include "grid_loader.h"
#include "protocol.h"

using namespace Bomberman;

namespace
{

Grid smallGrid()
{
    Grid grid(5, 4);
    grid.setTile({0, 0}, Tile(TileType::Wall));
    grid.setTile({2, 1}, Tile(TileType::DestructibleBlock));
    grid.addSpawnPoint({1, 1});
    grid.addSpawnPoint({3, 2});
    grid.setTheme("island");
    return grid;
}

void expectSameGrid(const Grid& actual, const Grid& expected)
{
    ASSERT_EQ(actual.getWidth(), expected.getWidth());
    ASSERT_EQ(actual.getHeight(), expected.getHeight());
    for (int y = 0; y < expected.getHeight(); ++y)
        for (int x = 0; x < expected.getWidth(); ++x)
            EXPECT_EQ(actual.getTile({x, y}), expected.getTile({x, y})) << x << "," << y;
    EXPECT_EQ(actual.getSpawnPoints(), expected.getSpawnPoints());
    EXPECT_EQ(actual.getTheme(), expected.getTheme());
}

// Encodes then decodes, as if the message had crossed the network.
template <typename T>
T roundTrip(const T& message)
{
    sf::Packet packet = encode(message);
    sf::Packet received;
    received.append(packet.getData(), packet.getDataSize());
    const std::optional<Message> decoded = decode(received);
    EXPECT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded && std::holds_alternative<T>(*decoded));
    return decoded && std::holds_alternative<T>(*decoded) ? std::get<T>(*decoded) : T{};
}

sf::Packet rawPacket(std::initializer_list<std::uint8_t> bytes)
{
    sf::Packet packet;
    for (const std::uint8_t byte : bytes)
        packet << byte;
    return packet;
}

} // namespace

TEST(ProtocolTest, HelloWelcomeAndRefusedRoundTrip)
{
    const Hello hello = roundTrip(Hello{ProtocolVersion, 4});
    EXPECT_EQ(hello.version, ProtocolVersion);
    EXPECT_EQ(hello.model, 4);
    EXPECT_EQ(roundTrip(Welcome{3}).seat, 3);
    EXPECT_EQ(roundTrip(Refused{RefusalReason::MatchInProgress}).reason, RefusalReason::MatchInProgress);
    EXPECT_EQ(roundTrip(ChangeModel{-1}).step, -1);
    (void)roundTrip(BackToLobby{});
}

TEST(ProtocolTest, LobbyUpdateCarriesTheSeatsAndTheMap)
{
    LobbyUpdate update;
    update.seats[0] = {true, 2};
    update.seats[2] = {true, 5};
    update.map = smallGrid();

    const LobbyUpdate received = roundTrip(update);
    EXPECT_EQ(received.seats, update.seats);
    expectSameGrid(received.map, update.map);
}

TEST(ProtocolTest, StartMatchCarriesEverythingToBuildTheSameEngine)
{
    StartMatch start;
    start.rules.seed = 0x0123'4567'89AB'CDEFull;
    start.rules.moveTicks = 12;
    start.rules.upgrades.dropChancePercent = 40;
    start.rules.upgrades.weights = {3, 0, 1};
    start.rules.startingStats.blastRadius = 2;
    start.grid = smallGrid();
    start.players = {{0, 1}, {2, 3}};
    start.you = 1;

    const StartMatch received = roundTrip(start);
    EXPECT_EQ(received.rules.seed, start.rules.seed);
    EXPECT_EQ(received.rules.tickRate, start.rules.tickRate);
    EXPECT_EQ(received.rules.moveTicks, 12);
    EXPECT_EQ(received.rules.turnTicks, start.rules.turnTicks);
    EXPECT_EQ(received.rules.upgrades.dropChancePercent, 40);
    EXPECT_EQ(received.rules.upgrades.weights, start.rules.upgrades.weights);
    EXPECT_EQ(received.rules.startingStats, start.rules.startingStats);
    expectSameGrid(received.grid, start.grid);
    EXPECT_EQ(received.players, start.players);
    EXPECT_EQ(received.you, 1);

    // Same grid + rules: both engines play the same match.
    EXPECT_EQ(GameEngine(received.grid, received.rules).stateHash(), GameEngine(start.grid, start.rules).stateHash());
}

TEST(ProtocolTest, InputCarriesTheHeldCommandOrNothing)
{
    EXPECT_EQ(roundTrip(Input{CharacterCommand::move(Direction::Left)}).command,
              CharacterCommand::move(Direction::Left));
    EXPECT_EQ(roundTrip(Input{CharacterCommand::placeBomb()}).command, CharacterCommand::placeBomb());
    EXPECT_EQ(roundTrip(Input{std::nullopt}).command, std::nullopt);
}

TEST(ProtocolTest, TicksCarryConsecutiveFramesAndAChecksum)
{
    Ticks ticks;
    ticks.frames = {TickFrame{300, {{0, CharacterCommand::move(Direction::Up)}, {2, CharacterCommand::pushBomb()}}},
                    TickFrame{301, {}}, TickFrame{302, {{1, CharacterCommand::placeBomb()}}}};
    ticks.checksum = StateChecksum{303, 0xDEAD'BEEF'0000'0001ull};

    const Ticks received = roundTrip(ticks);
    EXPECT_EQ(received.frames, ticks.frames);
    ASSERT_TRUE(received.checksum.has_value());
    EXPECT_EQ(received.checksum->tick, 303u);
    EXPECT_EQ(received.checksum->hash, ticks.checksum->hash);

    EXPECT_TRUE(roundTrip(Ticks{}).frames.empty());
}

TEST(ProtocolTest, MalformedPacketsAreRejected)
{
    sf::Packet empty;
    EXPECT_FALSE(decode(empty).has_value());
    sf::Packet unknownType = rawPacket({200});
    EXPECT_FALSE(decode(unknownType).has_value());
    sf::Packet truncated = rawPacket({static_cast<std::uint8_t>(MessageType::Hello), 1});
    EXPECT_FALSE(decode(truncated).has_value());
    sf::Packet badSeat = rawPacket({static_cast<std::uint8_t>(MessageType::Welcome), 9});
    EXPECT_FALSE(decode(badSeat).has_value());
    sf::Packet badCommand = rawPacket({static_cast<std::uint8_t>(MessageType::Input), 1, 7, 0});
    EXPECT_FALSE(decode(badCommand).has_value());

    sf::Packet trailing = encode(Welcome{1});
    trailing << std::uint8_t{0};
    EXPECT_FALSE(decode(trailing).has_value());
}

TEST(ProtocolTest, AbsurdGridsAreRejected)
{
    sf::Packet tooBig;
    tooBig << std::uint8_t{200} << std::uint8_t{200};
    EXPECT_FALSE(readGrid(tooBig).has_value());

    sf::Packet spawnOutside;
    spawnOutside << std::uint8_t{1} << std::uint8_t{1} << std::uint8_t{0} << std::uint8_t{1} << std::uint8_t{5}
                 << std::uint8_t{0} << std::string("deck");
    EXPECT_FALSE(readGrid(spawnOutside).has_value());
}

TEST(ProtocolTest, EveryShippedLevelCrossesTheNetworkIntact)
{
    for (int i = 1; i <= 4; ++i)
    {
        const Grid level = GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_0" +
                                                    std::to_string(i) + ".json");
        sf::Packet packet;
        writeGrid(packet, level);
        const std::optional<Grid> received = readGrid(packet);
        ASSERT_TRUE(received.has_value()) << "level " << i;
        expectSameGrid(*received, level);
    }
}

TEST(ProtocolTest, BeaconsRoundTripAndForeignDatagramsAreIgnored)
{
    Beacon beacon;
    beacon.gamePort = 50123;
    beacon.hostModel = 5;
    beacon.seatsTaken = 2;
    beacon.seatCount = 4;
    beacon.map = smallGrid();

    sf::Packet packet = encodeBeacon(beacon);
    const std::optional<Beacon> received = decodeBeacon(packet);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->gamePort, 50123);
    EXPECT_EQ(received->hostModel, 5);
    EXPECT_EQ(received->seatsTaken, 2);
    EXPECT_EQ(received->seatCount, 4);
    expectSameGrid(received->map, beacon.map);

    sf::Packet foreign;
    foreign << std::uint32_t{0x12345678} << std::string("hello");
    EXPECT_FALSE(decodeBeacon(foreign).has_value());
}

TEST(ProtocolTest, TickFramesGroupTheRecordedCommandsByTick)
{
    const std::vector<ExecutedCommand> executed = {{10, 0, CharacterCommand::move(Direction::Up)},
                                                   {10, 1, CharacterCommand::placeBomb()},
                                                   {12, 1, CharacterCommand::move(Direction::Left)}};
    const std::vector<TickFrame> frames = makeTickFrames(10, 3, executed);
    ASSERT_EQ(frames.size(), 3u);
    EXPECT_EQ(frames[0], (TickFrame{10, {{0, CharacterCommand::move(Direction::Up)}, {1, CharacterCommand::placeBomb()}}}));
    EXPECT_EQ(frames[1], (TickFrame{11, {}}));
    EXPECT_EQ(frames[2], (TickFrame{12, {{1, CharacterCommand::move(Direction::Left)}}}));
}

TEST(ProtocolTest, AFrameOnlyAppliesAtItsTick)
{
    Grid grid = smallGrid();
    GameEngine engine(grid);
    (void)engine.addPlayer();

    EXPECT_FALSE(applyTickFrame(engine, TickFrame{5, {}}));
    EXPECT_EQ(engine.getTick(), 0u);
    EXPECT_TRUE(applyTickFrame(engine, TickFrame{0, {{0, CharacterCommand::move(Direction::Right)}}}));
    EXPECT_EQ(engine.getTick(), 1u);
    EXPECT_EQ(engine.getCharacter(0).getFacing(), Direction::Right); // a block on (2, 1): it only turned
}

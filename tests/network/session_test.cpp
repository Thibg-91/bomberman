#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <SFML/Network/TcpSocket.hpp>

#include "client_session.h"
#include "connection.h"
#include "discovery.h"
#include "game_engine.h"
#include "grid_loader.h"
#include "host_session.h"
#include "random.h"
#include "tick_playout.h"

using namespace Bomberman;

// Host and clients in the same process, over the loopback interface: real
// sockets, real protocol, no window.
namespace
{

constexpr int CrewSize = 6;
constexpr float Frame = 1.f / 60.f;

Grid level()
{
    return GridLoader::loadFromFile(std::string(BOMBERMAN_DATA_DIR) + "/grid/level_01.json");
}

// A host on a free port, its beacons sent to nobody in particular.
std::unique_ptr<HostSession> makeHost(int hostModel = 0, Grid map = level())
{
    auto host = std::make_unique<HostSession>(std::move(map), CrewSize, hostModel, sf::Socket::AnyPort,
                                              sf::IpAddress::LocalHost, DiscoveryPort + 100);
    EXPECT_TRUE(host->isListening());
    return host;
}

std::unique_ptr<ClientSession> join(const HostSession& host, int model)
{
    return std::make_unique<ClientSession>(sf::IpAddress::LocalHost, host.getPort(), model);
}

// Updates everybody until condition holds (or 3 s pass).
bool pump(HostSession& host, const std::vector<ClientSession*>& clients, const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline)
    {
        host.update(Frame);
        for (ClientSession* client : clients)
            client->update();
        if (condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

bool seated(const ClientSession& client)
{
    return client.getState() == ClientSession::State::Connected && client.getLobby().has_value();
}

GameEngine engineFor(const StartMatch& start)
{
    GameEngine engine(start.grid, start.rules);
    for (std::size_t i = 0; i < start.players.size(); ++i)
        (void)engine.addPlayer();
    return engine;
}

} // namespace

TEST(SessionTest, AClientJoinsTheWaitingRoom)
{
    auto host = makeHost(0);
    auto client = join(*host, 3);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client) && client->getLobby()->seats[1].taken; }));

    EXPECT_EQ(client->getSeat(), 1);
    EXPECT_EQ(host->countClients(), 1);
    EXPECT_EQ(host->getLobby().getModel(1), 3);
    const LobbyUpdate& lobby = *client->getLobby();
    EXPECT_EQ(lobby.seats[0], (SeatInfo{true, 0}));
    EXPECT_EQ(lobby.seats[1], (SeatInfo{true, 3}));
    EXPECT_FALSE(lobby.seats[2].taken);
    EXPECT_EQ(lobby.map.getTheme(), host->getMap().getTheme());
}

TEST(SessionTest, EveryPlayerGetsADistinctModel)
{
    auto host = makeHost(2);
    auto first = join(*host, 2);
    auto second = join(*host, 2);
    ASSERT_TRUE(pump(*host, {first.get(), second.get()}, [&] { return host->getLobby().countPlayers() == 3; }));
    EXPECT_EQ(host->getLobby().getModel(1), 3);
    EXPECT_EQ(host->getLobby().getModel(2), 4);

    first->changeModel(1); // 4 is taken: 5
    ASSERT_TRUE(pump(*host, {first.get(), second.get()}, [&] {
        return second->getLobby() && second->getLobby()->seats[1] == SeatInfo{true, 5};
    }));
    EXPECT_TRUE(host->changeHostModel(-1)); // 1
    ASSERT_TRUE(pump(*host, {first.get(), second.get()}, [&] {
        return first->getLobby() && first->getLobby()->seats[0] == SeatInfo{true, 1};
    }));
}

TEST(SessionTest, AFullRoomRefusesNewcomers)
{
    Grid twoSpawns(5, 5);
    twoSpawns.addSpawnPoint({1, 1});
    twoSpawns.addSpawnPoint({3, 3});
    auto host = makeHost(0, twoSpawns);
    auto first = join(*host, 1);
    ASSERT_TRUE(pump(*host, {first.get()}, [&] { return seated(*first); }));

    auto late = join(*host, 2);
    ASSERT_TRUE(pump(*host, {first.get(), late.get()},
                     [&] { return late->getState() == ClientSession::State::Refused; }));
    EXPECT_EQ(late->getRefusal(), RefusalReason::Full);
}

TEST(SessionTest, AnotherProtocolVersionIsRefused)
{
    auto host = makeHost();
    auto socket = std::make_unique<sf::TcpSocket>();
    ASSERT_EQ(socket->connect(sf::IpAddress::LocalHost, host->getPort(), sf::seconds(1)), sf::Socket::Status::Done);
    Connection old(std::move(socket));
    old.send(Hello{static_cast<std::uint16_t>(ProtocolVersion + 1), 0});

    std::vector<Message> received;
    ASSERT_TRUE(pump(*host, {}, [&] {
        for (Message& message : old.receive())
            received.push_back(std::move(message));
        return !received.empty();
    }));
    ASSERT_TRUE(std::holds_alternative<Refused>(received.front()));
    EXPECT_EQ(std::get<Refused>(received.front()).reason, RefusalReason::VersionMismatch);
    EXPECT_EQ(host->getLobby().countPlayers(), 1);
}

TEST(SessionTest, ALeavingClientFreesItsSeat)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    client->disconnect();
    ASSERT_TRUE(pump(*host, {}, [&] { return host->getLobby().countPlayers() == 1; }));
    EXPECT_EQ(host->countClients(), 0);
}

TEST(SessionTest, TheMatchCannotStartAlone)
{
    auto host = makeHost();
    EXPECT_FALSE(host->startMatch(GameRules{}).has_value());
    EXPECT_EQ(host->getPhase(), HostSession::Phase::Lobby);
}

TEST(SessionTest, NewcomersAreRefusedDuringAMatch)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    ASSERT_TRUE(host->startMatch(GameRules{}).has_value());

    auto late = join(*host, 2);
    ASSERT_TRUE(pump(*host, {client.get(), late.get()},
                     [&] { return late->getState() == ClientSession::State::Refused; }));
    EXPECT_EQ(late->getRefusal(), RefusalReason::MatchInProgress);
}

TEST(SessionTest, EveryClientReceivesItsOwnPlayer)
{
    auto host = makeHost(0);
    auto first = join(*host, 1);
    auto second = join(*host, 2);
    ASSERT_TRUE(pump(*host, {first.get(), second.get()}, [&] { return seated(*first) && seated(*second); }));

    GameRules rules;
    rules.seed = 99;
    const std::optional<StartMatch> hostStart = host->startMatch(rules);
    ASSERT_TRUE(hostStart.has_value());
    EXPECT_EQ(hostStart->you, 0);
    EXPECT_EQ(hostStart->players, (std::vector<MatchPlayer>{{0, 0}, {1, 1}, {2, 2}}));

    std::optional<StartMatch> firstStart;
    std::optional<StartMatch> secondStart;
    ASSERT_TRUE(pump(*host, {first.get(), second.get()}, [&] {
        if (!firstStart)
            firstStart = first->takeStart();
        if (!secondStart)
            secondStart = second->takeStart();
        return firstStart && secondStart;
    }));
    EXPECT_EQ(firstStart->you, 1);
    EXPECT_EQ(secondStart->you, 2);
    EXPECT_EQ(firstStart->rules.seed, 99u);
    EXPECT_EQ(secondStart->players, hostStart->players);
}

TEST(SessionTest, HeldInputsReachTheHostAndShortTapsAreNotLost)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    ASSERT_TRUE(host->startMatch(GameRules{}).has_value());
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return client->takeStart().has_value(); }));

    client->sendInput(CharacterCommand::move(Direction::Left));
    std::optional<CharacterCommand> held;
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return (held = host->takeCommand(1)).has_value(); }));
    EXPECT_EQ(held, CharacterCommand::move(Direction::Left));
    EXPECT_EQ(host->takeCommand(1), CharacterCommand::move(Direction::Left)); // still held

    // A tap: pressed and released before the host reads it.
    client->sendInput(CharacterCommand::placeBomb());
    client->sendInput(std::nullopt);
    std::optional<CharacterCommand> tapped;
    ASSERT_TRUE(pump(*host, {client.get()}, [&] {
        tapped = host->takeCommand(1);
        return tapped == CharacterCommand::placeBomb() || !tapped;
    }));
    EXPECT_EQ(tapped, CharacterCommand::placeBomb());
    // Released: nothing once the release has arrived.
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return !host->takeCommand(1).has_value(); }));

    EXPECT_EQ(host->takeCommand(0), std::nullopt); // the host's keyboard is local
    EXPECT_TRUE(host->isPlayerConnected(1));
}

// The whole lockstep: the host plays a match with scripted inputs (its own
// and two clients' sent over the network), the clients replay the Ticks on
// their engines and must stay identical to the host, checksums included.
TEST(SessionTest, ClientsReplayTheHostMatchExactly)
{
    auto host = makeHost(0);
    auto first = join(*host, 1);
    auto second = join(*host, 2);
    std::vector<ClientSession*> clients = {first.get(), second.get()};
    ASSERT_TRUE(pump(*host, clients, [&] { return seated(*first) && seated(*second); }));

    GameRules rules;
    rules.seed = 1234;
    rules.upgrades.dropChancePercent = 50;
    const StartMatch hostStart = *host->startMatch(rules);
    GameEngine hostEngine = engineFor(hostStart);
    hostEngine.setCommandRecording(true);

    std::vector<std::optional<StartMatch>> starts(clients.size());
    ASSERT_TRUE(pump(*host, clients, [&] {
        for (std::size_t i = 0; i < clients.size(); ++i)
            if (!starts[i])
                starts[i] = clients[i]->takeStart();
        return starts[0] && starts[1];
    }));
    std::vector<GameEngine> clientEngines;
    for (const std::optional<StartMatch>& start : starts)
        clientEngines.push_back(engineFor(*start));
    std::vector<TickPlayout> playouts(clients.size());
    int checksumsVerified = 0;

    Random script(5);
    auto scripted = [&script]() -> std::optional<CharacterCommand> {
        switch (script.below(8))
        {
        case 0:
            return std::nullopt;
        case 1:
            return CharacterCommand::placeBomb();
        case 2:
            return CharacterCommand::pushBomb();
        default:
            return CharacterCommand::move(static_cast<Direction>(script.below(4)));
        }
    };

    // 10 s of play; host frames of uneven length (1 to 3 ticks).
    for (int frame = 0; frame < 300; ++frame)
    {
        for (ClientSession* client : clients)
            client->sendInput(scripted());
        host->update(Frame);

        if (const auto command = scripted())
            hostEngine.submitCommand(0, *command);
        for (PlayerId id = 1; id < hostEngine.getPlayerCount(); ++id)
            if (const auto command = host->takeCommand(id))
                hostEngine.submitCommand(id, *command);

        const std::uint64_t firstTick = hostEngine.getTick();
        const int ran = hostEngine.advance(Frame * static_cast<float>(1 + frame % 3));
        Ticks ticks;
        ticks.frames = makeTickFrames(firstTick, ran, hostEngine.takeExecutedCommands());
        if (firstTick / ChecksumInterval != hostEngine.getTick() / ChecksumInterval)
            ticks.checksum = StateChecksum{hostEngine.getTick(), hostEngine.stateHash()};
        if (ran > 0)
            host->sendTicks(ticks);
        (void)hostEngine.takeEvents();

        for (std::size_t i = 0; i < clients.size(); ++i)
        {
            clients[i]->update();
            for (const Ticks& received : clients[i]->takeTicks())
                playouts[i].push(received);
            for (const TickFrame& tickFrame : playouts[i].take(Frame))
            {
                ASSERT_TRUE(applyTickFrame(clientEngines[i], tickFrame));
                if (const auto expected = playouts[i].takeChecksum(clientEngines[i].getTick()))
                {
                    ASSERT_EQ(clientEngines[i].stateHash(), *expected) << "client " << i;
                    ++checksumsVerified;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    // Let the clients run what is still in flight, then compare.
    ASSERT_TRUE(pump(*host, clients, [&] {
        bool done = true;
        for (std::size_t i = 0; i < clients.size(); ++i)
        {
            for (const Ticks& received : clients[i]->takeTicks())
                playouts[i].push(received);
            for (const TickFrame& tickFrame : playouts[i].take(1.f))
                (void)applyTickFrame(clientEngines[i], tickFrame);
            for (const TickFrame& tickFrame : playouts[i].take(1.f))
                (void)applyTickFrame(clientEngines[i], tickFrame);
            done = done && clientEngines[i].getTick() == hostEngine.getTick();
        }
        return done;
    }));
    for (const GameEngine& engine : clientEngines)
        EXPECT_EQ(engine.stateHash(), hostEngine.stateHash());
    EXPECT_GT(checksumsVerified, 4);
}

TEST(SessionTest, AnAbandonedMatchSendsEveryoneBackToTheWaitingRoom)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    ASSERT_TRUE(host->startMatch(GameRules{}).has_value());
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return client->takeStart().has_value(); }));

    host->returnToLobby(true);
    EXPECT_EQ(host->getPhase(), HostSession::Phase::Lobby);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return client->takeBackToLobby(); }));
    EXPECT_EQ(host->takeCommand(1), std::nullopt);

    // And a new match can start.
    ASSERT_TRUE(host->startMatch(GameRules{}).has_value());
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return client->takeStart().has_value(); }));
}

TEST(SessionTest, AClientLeavingDuringAMatchLeavesItsPirateIdle)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    ASSERT_TRUE(host->startMatch(GameRules{}).has_value());
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return client->takeStart().has_value(); }));
    client->sendInput(CharacterCommand::move(Direction::Right));
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return host->takeCommand(1).has_value(); }));

    client->disconnect();
    ASSERT_TRUE(pump(*host, {}, [&] { return !host->isPlayerConnected(1); }));
    EXPECT_EQ(host->takeCommand(1), std::nullopt); // no more input: it stands still
    EXPECT_EQ(host->getPhase(), HostSession::Phase::Match);
    EXPECT_EQ(host->getLobby().countPlayers(), 1); // its seat is free for the next match
}

TEST(SessionTest, AClientNoticesWhenTheHostLeaves)
{
    auto host = makeHost();
    auto client = join(*host, 1);
    ASSERT_TRUE(pump(*host, {client.get()}, [&] { return seated(*client); }));
    host.reset();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (client->getState() != ClientSession::State::Disconnected && std::chrono::steady_clock::now() < deadline)
    {
        client->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(client->getState(), ClientSession::State::Disconnected);
}

TEST(SessionTest, ConnectingToNobodyFails)
{
    unsigned short freePort = 0;
    {
        sf::TcpListener probe;
        ASSERT_EQ(probe.listen(sf::Socket::AnyPort), sf::Socket::Status::Done);
        freePort = probe.getLocalPort();
    } // closed: nobody listens there any more
    ClientSession client(sf::IpAddress::LocalHost, freePort, 0, sf::milliseconds(500));
    EXPECT_EQ(client.getState(), ClientSession::State::Disconnected);
}

TEST(DiscoveryTest, AWaitingHostIsHeardAndForgottenOnceGone)
{
    DiscoveryListener listener(sf::Socket::AnyPort);
    ASSERT_TRUE(listener.isListening());
    auto host = std::make_unique<HostSession>(level(), CrewSize, 4, sf::Socket::AnyPort, sf::IpAddress::LocalHost,
                                              listener.getPort());

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (listener.getHosts().empty() && std::chrono::steady_clock::now() < deadline)
    {
        host->update(Frame);
        listener.update(Frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_EQ(listener.getHosts().size(), 1u);
    const DiscoveredHost& heard = listener.getHosts().front();
    EXPECT_EQ(heard.address, sf::IpAddress::LocalHost);
    EXPECT_EQ(heard.beacon.gamePort, host->getPort());
    EXPECT_EQ(heard.beacon.hostModel, 4);
    EXPECT_EQ(heard.beacon.seatsTaken, 1);
    EXPECT_EQ(heard.beacon.map.getTheme(), level().getTheme());

    // The host is heard again and again, but listed once.
    for (int i = 0; i < 60; ++i)
    {
        host->update(Frame);
        listener.update(Frame);
    }
    EXPECT_EQ(listener.getHosts().size(), 1u);

    host.reset();
    listener.update(0.f); // beacons still in flight
    listener.update(DiscoveryListener::Timeout + 0.1f);
    EXPECT_TRUE(listener.getHosts().empty());
}

TEST(DiscoveryTest, AHostInAMatchStopsAnnouncingItself)
{
    DiscoveryListener listener(sf::Socket::AnyPort);
    ASSERT_TRUE(listener.isListening());
    HostSession host(level(), CrewSize, 0, sf::Socket::AnyPort, sf::IpAddress::LocalHost, listener.getPort());
    ClientSession client(sf::IpAddress::LocalHost, host.getPort(), 1);
    ASSERT_TRUE(pump(host, {&client}, [&] { return seated(client); }));
    ASSERT_TRUE(host.startMatch(GameRules{}).has_value());

    listener.update(0.f); // beacons still in flight
    listener.update(DiscoveryListener::Timeout + 0.1f); // forget what was heard before
    for (int i = 0; i < 120; ++i)
    {
        host.update(Frame);
        listener.update(0.f);
    }
    EXPECT_TRUE(listener.getHosts().empty());
}

#ifndef HOST_SESSION_H
#define HOST_SESSION_H

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/TcpListener.hpp>

#include "character_command.h"
#include "connection.h"
#include "discovery.h"
#include "game_rules.h"
#include "grid.h"
#include "lobby.h"
#include "protocol.h"

namespace Bomberman
{

// The host's side of a network game (see doc/network.md). It listens for
// clients (TCP), announces itself on the local network while waiting
// (DiscoveryBeacon), runs the waiting room (Lobby) and, during a match,
// collects the clients' held commands and relays the executed ones (Ticks).
// It never blocks: exe/main.cpp calls update() once per frame.
//
// The host itself is the player on seat 0 (PlayerId 0). In a match,
// players are the taken seats in increasing order: PlayerId = rank of the
// seat. A client that leaves during a match leaves its pirate idle on the
// board (it stays a target); its seat is free again in the waiting room.
class HostSession
{
public:
    enum class Phase
    {
        Lobby,
        Match
    };

    static constexpr float HelloTimeout = 5.f; // seconds for a new connection to say Hello

    // =========================
    // Constructors / Destructor
    // =========================
    // map: what the match will be played on (its spawn points cap the seat
    // count). port: the TCP port to listen on; when it is taken another
    // free one is used (see getPort(), announced by the beacons). The
    // beacon target/port are only changed by tests.
    HostSession(Grid map, int modelCount, int hostModel, unsigned short port = DefaultGamePort,
                sf::IpAddress beaconTarget = sf::IpAddress::Broadcast, unsigned short beaconPort = DiscoveryPort);

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] bool isListening() const noexcept;
    [[nodiscard]] unsigned short getPort() const noexcept;
    [[nodiscard]] Phase getPhase() const noexcept;
    [[nodiscard]] const Lobby& getLobby() const noexcept;
    [[nodiscard]] const Grid& getMap() const noexcept;
    // Clients connected and seated.
    [[nodiscard]] int countClients() const noexcept;

    // Accepts newcomers, reads every client, sends what is pending, and
    // announces the room while in the Lobby phase.
    void update(float deltaSeconds);

    // The host picks its next/previous free model (Lobby phase).
    bool changeHostModel(int step);

    // Starts the match (needs Lobby::canStart()): every client receives its
    // StartMatch; returns the host's own one (you = 0). rules.seed must be
    // chosen by the caller.
    std::optional<StartMatch> startMatch(const GameRules& rules);

    // What client player id holds right now, to submit to the engine this
    // frame like a local key: its held command, or else one it pressed and
    // released since the last call (a short tap is never lost). Nothing for
    // the host's id, unknown ids and players whose client left.
    [[nodiscard]] std::optional<CharacterCommand> takeCommand(PlayerId id);
    // Whether the client driving player id is still connected.
    [[nodiscard]] bool isPlayerConnected(PlayerId id) const noexcept;

    // Relays what the host engine ran during this frame to every client of
    // the match.
    void sendTicks(const Ticks& ticks);

    // Back to the waiting room. notifyClients: the match was abandoned (the
    // clients leave it at once); a finished match does not need it, every
    // client shows its end screen then comes back on its own.
    void returnToLobby(bool notifyClients);

private:
    struct Client
    {
        explicit Client(std::unique_ptr<sf::TcpSocket> socket) : connection(std::move(socket)) {}

        Connection connection;
        std::optional<int> seat;        // set once its Hello is accepted
        std::optional<PlayerId> player; // during a match
        std::optional<CharacterCommand> held;
        std::optional<CharacterCommand> latched; // last command pressed, until taken
        float waitingForHello = 0.f;
    };

    // =========================
    // Private Methods
    // =========================
    void acceptNewcomers();
    void handle(Client& client, const Message& message);
    void refuse(Client& client, RefusalReason reason);
    void dropClosedClients();
    void broadcastLobby();
    [[nodiscard]] Beacon makeBeacon() const;

    // =========================
    // Members
    // =========================
    sf::TcpListener m_listener;
    bool m_listening = false;
    Grid m_map;
    Lobby m_lobby;
    Phase m_phase = Phase::Lobby;
    bool m_lobbyChanged = true;
    DiscoveryBeacon m_beacon;
    std::vector<std::unique_ptr<Client>> m_clients;
};

} // namespace Bomberman

#endif // HOST_SESSION_H

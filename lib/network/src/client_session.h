#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#pragma once

#include <optional>
#include <vector>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/System/Time.hpp>

#include "character_command.h"
#include "connection.h"
#include "protocol.h"

namespace Bomberman
{

// A client's side of a network game (see doc/network.md): connects to a
// host, says Hello, then follows the waiting room (LobbyUpdate) and the
// matches (StartMatch, Ticks). exe/main.cpp calls update() once per frame
// and takes what arrived.
class ClientSession
{
public:
    enum class State
    {
        Connecting, // connected, waiting for the host's Welcome
        Connected,  // seated in the waiting room (or playing)
        Refused,    // see getRefusal()
        Disconnected
    };

    static constexpr float DefaultConnectTimeout = 3.f; // seconds

    // =========================
    // Constructors / Destructor
    // =========================
    // Connects at once (blocking for at most connectTimeout: on a local
    // network an answer is immediate, a wrong address takes the whole
    // timeout) and says Hello with the wanted model (index in
    // AllCharacterModels).
    ClientSession(sf::IpAddress host, unsigned short port, int wantedModel,
                  sf::Time connectTimeout = sf::seconds(DefaultConnectTimeout));

    // =========================
    // Public Methods
    // =========================
    void update();

    [[nodiscard]] State getState() const noexcept;
    [[nodiscard]] std::optional<RefusalReason> getRefusal() const noexcept;
    [[nodiscard]] std::optional<int> getSeat() const noexcept;
    // The last waiting room state received.
    [[nodiscard]] const std::optional<LobbyUpdate>& getLobby() const noexcept;

    // Asks the host for the next (step > 0) or previous free model.
    void changeModel(int step);

    // The match the host started, once (the newest one when several came).
    [[nodiscard]] std::optional<StartMatch> takeStart();
    // The Ticks received since the last call (oldest first).
    [[nodiscard]] std::vector<Ticks> takeTicks();
    // Whether the host abandoned the match since the last call.
    [[nodiscard]] bool takeBackToLobby();

    // Tells the host what the keyboard holds; only sent when it changes.
    void sendInput(const std::optional<CharacterCommand>& command);

    // Leaves the host (also done by the destructor through the socket).
    void disconnect();

private:
    // =========================
    // Members
    // =========================
    Connection m_connection;
    State m_state = State::Disconnected;
    std::optional<RefusalReason> m_refusal;
    std::optional<int> m_seat;
    std::optional<LobbyUpdate> m_lobby;
    std::optional<StartMatch> m_start;
    std::vector<Ticks> m_ticks;
    bool m_backToLobby = false;
    std::optional<std::optional<CharacterCommand>> m_lastInput; // empty before the first one
};

} // namespace Bomberman

#endif // CLIENT_SESSION_H

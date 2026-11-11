#include "client_session.h"

#include <memory>
#include <utility>

#include <SFML/Network/TcpSocket.hpp>

namespace Bomberman
{

namespace
{

std::unique_ptr<sf::TcpSocket> connectTo(sf::IpAddress host, unsigned short port, sf::Time timeout)
{
    auto socket = std::make_unique<sf::TcpSocket>();
    if (socket->connect(host, port, timeout) != sf::Socket::Status::Done)
        return nullptr;
    return socket;
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
ClientSession::ClientSession(sf::IpAddress host, unsigned short port, int wantedModel, sf::Time connectTimeout)
    : m_connection(connectTo(host, port, connectTimeout))
{
    if (!m_connection.isOpen())
        return;
    m_state = State::Connecting;
    m_connection.send(Hello{ProtocolVersion, static_cast<std::uint8_t>(wantedModel)});
}

// =========================
// Public Methods
// =========================
void ClientSession::update()
{
    for (Message& message : m_connection.receive())
    {
        if (const auto* welcome = std::get_if<Welcome>(&message))
        {
            m_seat = welcome->seat;
            m_state = State::Connected;
        }
        else if (const auto* refused = std::get_if<Refused>(&message))
        {
            m_refusal = refused->reason;
            m_state = State::Refused;
        }
        else if (auto* lobby = std::get_if<LobbyUpdate>(&message))
            m_lobby = std::move(*lobby);
        else if (auto* start = std::get_if<StartMatch>(&message))
        {
            m_start = std::move(*start);
            m_ticks.clear(); // leftovers of a previous match
            m_backToLobby = false;
            m_lastInput.reset();
        }
        else if (auto* ticks = std::get_if<Ticks>(&message))
            m_ticks.push_back(std::move(*ticks));
        else if (std::holds_alternative<BackToLobby>(message))
            m_backToLobby = true;
        else
            m_connection.close(); // hosts never send anything else
    }
    m_connection.flush();

    if (!m_connection.isOpen() && m_state != State::Refused)
        m_state = State::Disconnected;
}

ClientSession::State ClientSession::getState() const noexcept
{
    return m_state;
}

std::optional<RefusalReason> ClientSession::getRefusal() const noexcept
{
    return m_refusal;
}

std::optional<int> ClientSession::getSeat() const noexcept
{
    return m_seat;
}

const std::optional<LobbyUpdate>& ClientSession::getLobby() const noexcept
{
    return m_lobby;
}

void ClientSession::changeModel(int step)
{
    m_connection.send(ChangeModel{static_cast<std::int8_t>(step > 0 ? 1 : -1)});
}

std::optional<StartMatch> ClientSession::takeStart()
{
    return std::exchange(m_start, std::nullopt);
}

std::vector<Ticks> ClientSession::takeTicks()
{
    return std::exchange(m_ticks, {});
}

bool ClientSession::takeBackToLobby()
{
    return std::exchange(m_backToLobby, false);
}

void ClientSession::sendInput(const std::optional<CharacterCommand>& command)
{
    if (m_lastInput && *m_lastInput == command)
        return;
    m_lastInput = command;
    m_connection.send(Input{command});
}

void ClientSession::disconnect()
{
    m_connection.close();
    if (m_state != State::Refused)
        m_state = State::Disconnected;
}

} // namespace Bomberman

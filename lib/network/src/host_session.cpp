#include "host_session.h"

#include <algorithm>
#include <utility>

namespace Bomberman
{

namespace
{

int seatCountFor(const Grid& map)
{
    return std::clamp(static_cast<int>(map.getSpawnPoints().size()), 1, MaxPlayers);
}

} // namespace

// =========================
// Constructors / Destructor
// =========================
HostSession::HostSession(Grid map, int modelCount, int hostModel, unsigned short port, sf::IpAddress beaconTarget,
                         unsigned short beaconPort)
    : m_map(std::move(map)),
      m_lobby(modelCount, hostModel, seatCountFor(m_map)),
      m_beacon(beaconTarget, beaconPort)
{
    m_listener.setBlocking(false);
    m_listening = m_listener.listen(port) == sf::Socket::Status::Done;
    if (!m_listening && port != sf::Socket::AnyPort)
        m_listening = m_listener.listen(sf::Socket::AnyPort) == sf::Socket::Status::Done; // port taken
}

// =========================
// Public Methods
// =========================
bool HostSession::isListening() const noexcept
{
    return m_listening;
}

unsigned short HostSession::getPort() const noexcept
{
    return m_listening ? m_listener.getLocalPort() : 0;
}

HostSession::Phase HostSession::getPhase() const noexcept
{
    return m_phase;
}

const Lobby& HostSession::getLobby() const noexcept
{
    return m_lobby;
}

const Grid& HostSession::getMap() const noexcept
{
    return m_map;
}

int HostSession::countClients() const noexcept
{
    return static_cast<int>(std::count_if(m_clients.begin(), m_clients.end(), [](const std::unique_ptr<Client>& c) {
        return c->seat.has_value() && c->connection.isOpen();
    }));
}

void HostSession::update(float deltaSeconds)
{
    acceptNewcomers();

    for (const std::unique_ptr<Client>& client : m_clients)
    {
        for (const Message& message : client->connection.receive())
            handle(*client, message);
        if (!client->seat && client->connection.isOpen())
        {
            client->waitingForHello += deltaSeconds;
            if (client->waitingForHello > HelloTimeout)
                client->connection.close(); // not one of our clients
        }
    }
    dropClosedClients();

    if (m_phase == Phase::Lobby)
    {
        if (m_lobbyChanged)
            broadcastLobby();
        m_beacon.update(deltaSeconds, makeBeacon());
    }

    for (const std::unique_ptr<Client>& client : m_clients)
        client->connection.flush();
}

bool HostSession::changeHostModel(int step)
{
    if (m_phase != Phase::Lobby || !m_lobby.changeModel(0, step))
        return false;
    m_lobbyChanged = true;
    return true;
}

std::optional<StartMatch> HostSession::startMatch(const GameRules& rules)
{
    if (m_phase != Phase::Lobby || !m_lobby.canStart())
        return std::nullopt;

    StartMatch start;
    start.rules = rules;
    start.grid = m_map;
    const std::vector<int> seats = m_lobby.getTakenSeats();
    for (const int seat : seats)
        start.players.push_back(MatchPlayer{static_cast<std::uint8_t>(seat), static_cast<std::uint8_t>(*m_lobby.getModel(seat))});

    for (const std::unique_ptr<Client>& client : m_clients)
    {
        client->held.reset();
        client->latched.reset();
        client->player.reset();
        if (!client->seat)
            continue;
        const auto rank = std::find(seats.begin(), seats.end(), *client->seat) - seats.begin();
        client->player = static_cast<PlayerId>(rank);
        StartMatch theirs = start;
        theirs.you = *client->player;
        client->connection.send(theirs);
    }
    m_phase = Phase::Match;
    start.you = 0; // the host is on seat 0, the first taken one
    return start;
}

std::optional<CharacterCommand> HostSession::takeCommand(PlayerId id)
{
    for (const std::unique_ptr<Client>& client : m_clients)
    {
        if (client->player != id || !client->connection.isOpen())
            continue;
        std::optional<CharacterCommand> command = client->held ? client->held : client->latched;
        client->latched.reset();
        return command;
    }
    return std::nullopt;
}

bool HostSession::isPlayerConnected(PlayerId id) const noexcept
{
    if (id == 0)
        return true; // the host itself
    return std::any_of(m_clients.begin(), m_clients.end(), [id](const std::unique_ptr<Client>& client) {
        return client->player == id && client->connection.isOpen();
    });
}

void HostSession::sendTicks(const Ticks& ticks)
{
    for (const std::unique_ptr<Client>& client : m_clients)
        if (client->player)
            client->connection.send(ticks);
}

void HostSession::returnToLobby(bool notifyClients)
{
    for (const std::unique_ptr<Client>& client : m_clients)
    {
        if (notifyClients && client->player)
            client->connection.send(BackToLobby{});
        client->player.reset();
        client->held.reset();
        client->latched.reset();
    }
    m_phase = Phase::Lobby;
    m_lobbyChanged = true;
}

// =========================
// Private Methods
// =========================
void HostSession::acceptNewcomers()
{
    while (m_listening)
    {
        auto socket = std::make_unique<sf::TcpSocket>();
        if (m_listener.accept(*socket) != sf::Socket::Status::Done)
            break;
        m_clients.push_back(std::make_unique<Client>(std::move(socket)));
    }
}

void HostSession::handle(Client& client, const Message& message)
{
    if (const auto* hello = std::get_if<Hello>(&message))
    {
        if (client.seat)
            return; // already seated
        if (hello->version != ProtocolVersion)
            return refuse(client, RefusalReason::VersionMismatch);
        if (m_phase != Phase::Lobby)
            return refuse(client, RefusalReason::MatchInProgress);
        const std::optional<int> seat = m_lobby.join(hello->model);
        if (!seat)
            return refuse(client, RefusalReason::Full);
        client.seat = seat;
        client.connection.send(Welcome{static_cast<std::uint8_t>(*seat)});
        m_lobbyChanged = true;
    }
    else if (const auto* change = std::get_if<ChangeModel>(&message))
    {
        if (client.seat && m_phase == Phase::Lobby && m_lobby.changeModel(*client.seat, change->step))
            m_lobbyChanged = true;
    }
    else if (const auto* input = std::get_if<Input>(&message))
    {
        if (!client.player)
            return; // late input from a finished match
        client.held = input->command;
        if (input->command)
            client.latched = input->command;
    }
    else
        client.connection.close(); // clients never send anything else
}

void HostSession::refuse(Client& client, RefusalReason reason)
{
    client.connection.send(Refused{reason});
    client.connection.close();
}

void HostSession::dropClosedClients()
{
    const auto closed = std::remove_if(m_clients.begin(), m_clients.end(), [this](const std::unique_ptr<Client>& c) {
        if (c->connection.isOpen())
            return false;
        if (c->seat)
        {
            m_lobby.leave(*c->seat);
            m_lobbyChanged = true;
        }
        return true;
    });
    m_clients.erase(closed, m_clients.end());
}

void HostSession::broadcastLobby()
{
    m_lobbyChanged = false;
    LobbyUpdate update;
    update.seats = m_lobby.toSeatInfos();
    update.map = m_map;
    for (const std::unique_ptr<Client>& client : m_clients)
        if (client->seat)
            client->connection.send(update);
}

Beacon HostSession::makeBeacon() const
{
    Beacon beacon;
    beacon.gamePort = getPort();
    beacon.hostModel = static_cast<std::uint8_t>(m_lobby.getModel(0).value_or(0));
    beacon.seatsTaken = static_cast<std::uint8_t>(m_lobby.countPlayers());
    beacon.seatCount = static_cast<std::uint8_t>(m_lobby.getSeatCount());
    beacon.map = m_map;
    return beacon;
}

} // namespace Bomberman

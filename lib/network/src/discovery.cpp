#include "discovery.h"

#include <algorithm>
#include <optional>

#include <SFML/Network/Packet.hpp>

namespace Bomberman
{

// =========================
// DiscoveryBeacon
// =========================
DiscoveryBeacon::DiscoveryBeacon(sf::IpAddress target, unsigned short port)
    : m_target(target),
      m_port(port)
{
    m_socket.setBlocking(false);
}

void DiscoveryBeacon::update(float deltaSeconds, const Beacon& beacon)
{
    m_untilNext -= deltaSeconds;
    if (m_untilNext > 0.f)
        return;
    m_untilNext = Interval;
    sf::Packet packet = encodeBeacon(beacon);
    // Lost beacons do not matter: the next one follows shortly.
    (void)m_socket.send(packet, m_target, m_port);
}

// =========================
// DiscoveryListener
// =========================
DiscoveryListener::DiscoveryListener(unsigned short port)
{
    m_socket.setBlocking(false);
    m_listening = m_socket.bind(port) == sf::Socket::Status::Done;
}

bool DiscoveryListener::isListening() const noexcept
{
    return m_listening;
}

unsigned short DiscoveryListener::getPort() const
{
    return m_socket.getLocalPort();
}

void DiscoveryListener::update(float deltaSeconds)
{
    for (DiscoveredHost& host : m_hosts)
        host.silentFor += deltaSeconds;

    while (m_listening)
    {
        sf::Packet packet;
        std::optional<sf::IpAddress> sender;
        unsigned short senderPort = 0;
        if (m_socket.receive(packet, sender, senderPort) != sf::Socket::Status::Done)
            break;
        const std::optional<Beacon> beacon = decodeBeacon(packet);
        if (!sender || !beacon || beacon->version != ProtocolVersion)
            continue;
        const auto known = std::find_if(m_hosts.begin(), m_hosts.end(), [&](const DiscoveredHost& host) {
            return host.address == *sender && host.beacon.gamePort == beacon->gamePort;
        });
        if (known != m_hosts.end())
        {
            known->beacon = *beacon;
            known->silentFor = 0.f;
        }
        else
            m_hosts.push_back(DiscoveredHost{*sender, *beacon, 0.f});
    }

    m_hosts.erase(std::remove_if(m_hosts.begin(), m_hosts.end(),
                                 [](const DiscoveredHost& host) { return host.silentFor > Timeout; }),
                  m_hosts.end());
}

const std::vector<DiscoveredHost>& DiscoveryListener::getHosts() const noexcept
{
    return m_hosts;
}

} // namespace Bomberman

#ifndef DISCOVERY_H
#define DISCOVERY_H

#pragma once

#include <vector>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/UdpSocket.hpp>

#include "protocol.h"

namespace Bomberman
{

// Host side of the discovery: while the host waits for players, it
// broadcasts a Beacon on the local network every Interval seconds (UDP, port
// DiscoveryPort). Some networks drop broadcasts (guest/public Wi-Fi, client
// isolation): clients can then join with the host's address (--join).
class DiscoveryBeacon
{
public:
    static constexpr float Interval = 0.5f; // seconds between two beacons

    // =========================
    // Constructors / Destructor
    // =========================
    // target/port: where beacons go (the tests send them to LocalHost).
    explicit DiscoveryBeacon(sf::IpAddress target = sf::IpAddress::Broadcast, unsigned short port = DiscoveryPort);

    // =========================
    // Public Methods
    // =========================
    // Sends beacon when Interval has elapsed since the last one (at once on
    // the first call).
    void update(float deltaSeconds, const Beacon& beacon);

private:
    // =========================
    // Members
    // =========================
    sf::UdpSocket m_socket;
    sf::IpAddress m_target;
    unsigned short m_port;
    float m_untilNext = 0.f;
};

// A host heard on the network.
struct DiscoveredHost
{
    sf::IpAddress address;
    Beacon beacon;
    float silentFor = 0.f; // seconds since its last beacon
};

// Client side of the discovery: listens to the beacons and keeps the list
// of hosts heard during the last Timeout seconds, in the order they were
// first heard (so the selection does not jump around). Only hosts speaking
// our ProtocolVersion are listed.
class DiscoveryListener
{
public:
    static constexpr float Timeout = 2.5f; // seconds without beacon before a host is forgotten

    // =========================
    // Constructors / Destructor
    // =========================
    explicit DiscoveryListener(unsigned short port = DiscoveryPort);

    // =========================
    // Public Methods
    // =========================
    // False when the port could not be bound (another client already
    // listens on this computer, for instance): --join still works.
    [[nodiscard]] bool isListening() const noexcept;
    // The bound port (useful with sf::Socket::AnyPort, in tests).
    [[nodiscard]] unsigned short getPort() const;
    void update(float deltaSeconds);
    [[nodiscard]] const std::vector<DiscoveredHost>& getHosts() const noexcept;

private:
    // =========================
    // Members
    // =========================
    sf::UdpSocket m_socket;
    bool m_listening = false;
    std::vector<DiscoveredHost> m_hosts;
};

} // namespace Bomberman

#endif // DISCOVERY_H

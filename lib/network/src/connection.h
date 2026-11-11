#ifndef CONNECTION_H
#define CONNECTION_H

#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/Packet.hpp>
#include <SFML/Network/TcpSocket.hpp>

#include "protocol.h"

namespace Bomberman
{

// One TCP link to a peer, never blocking the game loop: the socket is
// non-blocking, messages to send wait in a queue until the socket accepts
// them (a packet partly sent is resumed at the next flush()), and receive()
// only returns complete messages. The connection closes itself when the
// peer leaves, sends a malformed message, or stops reading for so long that
// MaxQueuedPackets pile up (a frozen peer must not stall the others).
class Connection
{
public:
    static constexpr std::size_t MaxQueuedPackets = 1200; // ~20 s of Ticks at 60 frames per second

    // =========================
    // Constructors / Destructor
    // =========================
    explicit Connection(std::unique_ptr<sf::TcpSocket> socket);

    // =========================
    // Public Methods
    // =========================
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] std::optional<sf::IpAddress> getRemoteAddress() const;

    // Queues message and tries to send what is queued.
    void send(const Message& message);
    // Sends as much of the queue as the socket takes now.
    void flush();
    // Every complete message received since the last call, oldest first.
    [[nodiscard]] std::vector<Message> receive();
    // Sends what it can of the queue, then disconnects.
    void close();

private:
    // =========================
    // Members
    // =========================
    std::unique_ptr<sf::TcpSocket> m_socket;
    std::deque<sf::Packet> m_outbox;
    sf::Packet m_inbox; // keeps a partly received packet between calls
    bool m_open = true;
};

} // namespace Bomberman

#endif // CONNECTION_H

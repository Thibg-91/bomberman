#include "connection.h"

#include <utility>

namespace Bomberman
{

// =========================
// Constructors / Destructor
// =========================
Connection::Connection(std::unique_ptr<sf::TcpSocket> socket)
    : m_socket(std::move(socket))
{
    m_open = m_socket != nullptr;
    if (m_socket)
        m_socket->setBlocking(false);
}

// =========================
// Public Methods
// =========================
bool Connection::isOpen() const noexcept
{
    return m_open;
}

std::optional<sf::IpAddress> Connection::getRemoteAddress() const
{
    return m_socket ? m_socket->getRemoteAddress() : std::nullopt;
}

void Connection::send(const Message& message)
{
    if (!m_open)
        return;
    if (m_outbox.size() >= MaxQueuedPackets)
    {
        close(); // the peer stopped reading
        return;
    }
    m_outbox.push_back(encode(message));
    flush();
}

void Connection::flush()
{
    while (m_open && !m_outbox.empty())
    {
        // SFML remembers how much of a packet went out: a Partial packet
        // must be sent again, as is, until it is Done.
        switch (m_socket->send(m_outbox.front()))
        {
        case sf::Socket::Status::Done:
            m_outbox.pop_front();
            break;
        case sf::Socket::Status::Partial:
        case sf::Socket::Status::NotReady:
            return; // the socket buffer is full, retry at the next flush
        case sf::Socket::Status::Disconnected:
        case sf::Socket::Status::Error:
            m_open = false;
            m_outbox.clear();
            return;
        }
    }
}

std::vector<Message> Connection::receive()
{
    std::vector<Message> messages;
    while (m_open)
    {
        const sf::Socket::Status status = m_socket->receive(m_inbox);
        if (status == sf::Socket::Status::Done)
        {
            std::optional<Message> message = decode(m_inbox);
            m_inbox.clear();
            if (!message)
            {
                close(); // not our protocol
                break;
            }
            messages.push_back(std::move(*message));
            continue;
        }
        if (status == sf::Socket::Status::Disconnected || status == sf::Socket::Status::Error)
            m_open = false;
        break; // NotReady/Partial: the rest comes later
    }
    return messages;
}

void Connection::close()
{
    if (!m_socket)
        return;
    flush(); // e.g. a Refused message
    m_socket->disconnect();
    m_outbox.clear();
    m_open = false;
}

} // namespace Bomberman

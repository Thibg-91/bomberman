#ifndef PROTOCOL_H
#define PROTOCOL_H

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <SFML/Network/Packet.hpp>

#include "character_command.h"
#include "game_engine.h"
#include "game_rules.h"
#include "grid.h"

namespace Bomberman
{

// Network protocol of the multiplayer mode (see doc/network.md).
//
// Host-paced lockstep: only the host runs the match with authority. Each
// client sends its held keyboard command (Input), the host feeds it to its
// engine like a local key, and broadcasts after each frame the commands its
// engine executed, tick by tick (Ticks). Every client replays them on its
// own GameEngine, built from the same grid, rules and seed (StartMatch):
// same commands + same seed = same match, so the clients draw exactly what
// the host simulates without ever receiving the state itself. A state hash
// sent every second detects a desynchronisation.
//
// Every message is one sf::Packet (TCP keeps them whole and in order) made
// of a MessageType byte and its fields. Character models travel as their
// index in AllCharacterModels: this library does not depend on the render
// one.

inline constexpr std::uint16_t ProtocolVersion = 1;
inline constexpr std::uint32_t BeaconMagic = 0xB0B5'0A7Eu; // tells our beacons apart on the UDP port
inline constexpr unsigned short DefaultGamePort = 53790;  // TCP, the host listens
inline constexpr unsigned short DiscoveryPort = 53791;    // UDP, the clients listen to beacons
inline constexpr int MaxPlayers = 4;
// Ticks between two state hashes sent by the host (one per second at 60 Hz).
inline constexpr std::uint64_t ChecksumInterval = 60;

enum class MessageType : std::uint8_t
{
    Hello = 1,   // client -> host: protocol version and wanted model
    Welcome,     // host -> client: accepted, here is your seat
    Refused,     // host -> client: refused (then the host closes the connection)
    LobbyUpdate, // host -> clients: the seats and the map
    ChangeModel, // client -> host: next/previous free model
    StartMatch,  // host -> clients: everything to build the same GameEngine
    Input,       // client -> host: the command held right now (or none)
    Ticks,       // host -> clients: the commands executed during some ticks
    BackToLobby  // host -> clients: the match was abandoned, back to the waiting room
};

enum class RefusalReason : std::uint8_t
{
    Full = 1,
    VersionMismatch,
    MatchInProgress
};

struct Hello
{
    std::uint16_t version = ProtocolVersion;
    std::uint8_t model = 0; // wanted model (another free one if taken)
};

struct Welcome
{
    std::uint8_t seat = 0;
};

struct Refused
{
    RefusalReason reason = RefusalReason::Full;
};

// A seat of the waiting room; seat 0 is the host's.
struct SeatInfo
{
    bool taken = false;
    std::uint8_t model = 0;

    friend bool operator==(const SeatInfo& lhs, const SeatInfo& rhs) noexcept
    {
        return lhs.taken == rhs.taken && (!lhs.taken || lhs.model == rhs.model);
    }
};

struct LobbyUpdate
{
    std::array<SeatInfo, MaxPlayers> seats{};
    Grid map; // what the host will play on (shown as a miniature)
};

struct ChangeModel
{
    std::int8_t step = 1; // +1 next free model in crew order, -1 previous one
};

// A player of the match, by PlayerId (the index in StartMatch::players).
struct MatchPlayer
{
    std::uint8_t seat = 0; // its color (bandana of CharacterRenderer::paletteForPlayer(seat))
    std::uint8_t model = 0;

    friend bool operator==(const MatchPlayer& lhs, const MatchPlayer& rhs) noexcept
    {
        return lhs.seat == rhs.seat && lhs.model == rhs.model;
    }
};

struct StartMatch
{
    GameRules rules; // seed included
    Grid grid;
    std::vector<MatchPlayer> players; // added to the engine in this order
    PlayerId you = 0;                 // the receiver's player
};

struct Input
{
    std::optional<CharacterCommand> command; // what the keyboard holds right now
};

// The commands executed during one tick, by player.
struct TickFrame
{
    std::uint64_t tick = 0;
    std::vector<std::pair<PlayerId, CharacterCommand>> commands;

    friend bool operator==(const TickFrame& lhs, const TickFrame& rhs) noexcept
    {
        return lhs.tick == rhs.tick && lhs.commands == rhs.commands;
    }
};

struct StateChecksum
{
    std::uint64_t tick = 0; // GameEngine::getTick() when the hash was taken
    std::uint64_t hash = 0; // GameEngine::stateHash()
};

// The ticks the host ran during one of its frames, in order, without gaps.
struct Ticks
{
    std::vector<TickFrame> frames;
    std::optional<StateChecksum> checksum; // of the state after the last frame
};

struct BackToLobby
{
};

using Message = std::variant<Hello, Welcome, Refused, LobbyUpdate, ChangeModel, StartMatch, Input, Ticks, BackToLobby>;

// Announced on the local network by a host waiting for players (UDP
// broadcast, see DiscoveryBeacon); what the "search a ship" screen shows.
struct Beacon
{
    std::uint16_t version = ProtocolVersion;
    std::uint16_t gamePort = DefaultGamePort;
    std::uint8_t hostModel = 0;
    std::uint8_t seatsTaken = 1;
    std::uint8_t seatCount = MaxPlayers; // as many as the map has spawn points, at most MaxPlayers
    Grid map;
};

// =========================
// Encoding
// =========================
[[nodiscard]] sf::Packet encode(const Message& message);
// Nothing when the packet is malformed (unknown type, truncated, absurd
// sizes): a peer sending garbage is disconnected.
[[nodiscard]] std::optional<Message> decode(sf::Packet& packet);

[[nodiscard]] sf::Packet encodeBeacon(const Beacon& beacon);
[[nodiscard]] std::optional<Beacon> decodeBeacon(sf::Packet& packet);

// Grids travel whole (tiles, spawn points, theme): a client does not need
// the host's level files.
void writeGrid(sf::Packet& packet, const Grid& grid);
[[nodiscard]] std::optional<Grid> readGrid(sf::Packet& packet);

// =========================
// Lockstep helpers
// =========================
// Groups what the host engine recorded during ticks [firstTick, firstTick +
// count) into one frame per tick (empty ones included: a client must run
// every tick).
[[nodiscard]] std::vector<TickFrame> makeTickFrames(std::uint64_t firstTick, int count,
                                                    const std::vector<ExecutedCommand>& executed);

// Runs frame on a client engine: submits its commands, then one tick.
// Returns false (and does nothing) when the engine is not at frame.tick.
bool applyTickFrame(GameEngine& engine, const TickFrame& frame);

} // namespace Bomberman

#endif // PROTOCOL_H

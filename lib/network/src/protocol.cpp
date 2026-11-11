#include "protocol.h"

#include <algorithm>
#include <string>
#include <type_traits>

namespace Bomberman
{

namespace
{

// Sanity limits: anything bigger is a malformed (or hostile) packet.
constexpr int MaxGridSide = 64;
constexpr std::uint8_t MaxSpawnPoints = 16;
constexpr std::size_t MaxThemeLength = 32;
constexpr std::uint16_t MaxFramesPerMessage = 600; // 10 s of ticks
constexpr int MaxRuleValue = 100000;

// ---------- primitive helpers ----------
void writeCommand(sf::Packet& packet, const CharacterCommand& command)
{
    packet << static_cast<std::uint8_t>(command.type) << static_cast<std::uint8_t>(command.direction);
}

std::optional<CharacterCommand> readCommand(sf::Packet& packet)
{
    std::uint8_t type = 0;
    std::uint8_t direction = 0;
    if (!(packet >> type >> direction) || type > static_cast<std::uint8_t>(CharacterCommand::Type::PlaceBomb)
        || direction > static_cast<std::uint8_t>(Direction::Right))
        return std::nullopt;
    return CharacterCommand{static_cast<CharacterCommand::Type>(type), static_cast<Direction>(direction)};
}

void writeInt(sf::Packet& packet, int value)
{
    packet << static_cast<std::int32_t>(value);
}

bool readInt(sf::Packet& packet, int& value, int min = 0, int max = MaxRuleValue)
{
    std::int32_t raw = 0;
    if (!(packet >> raw) || raw < min || raw > max)
        return false;
    value = static_cast<int>(raw);
    return true;
}

void writeRules(sf::Packet& packet, const GameRules& rules)
{
    for (const int value : {rules.tickRate, rules.moveTicks, rules.turnTicks, rules.placeBombTicks,
                            rules.pushBombTicks, rules.startingStats.maxBombs, rules.startingStats.pushDistance,
                            rules.startingStats.blastRadius, rules.upgrades.dropChancePercent,
                            rules.upgrades.maxBombs, rules.upgrades.maxPushDistance, rules.upgrades.maxBlastRadius})
        writeInt(packet, value);
    for (const int weight : rules.upgrades.weights)
        writeInt(packet, weight);
    packet << static_cast<std::uint64_t>(rules.seed);
}

std::optional<GameRules> readRules(sf::Packet& packet)
{
    GameRules rules;
    for (int* value : {&rules.tickRate, &rules.moveTicks, &rules.turnTicks, &rules.placeBombTicks,
                       &rules.pushBombTicks, &rules.startingStats.maxBombs, &rules.startingStats.pushDistance,
                       &rules.startingStats.blastRadius, &rules.upgrades.dropChancePercent, &rules.upgrades.maxBombs,
                       &rules.upgrades.maxPushDistance, &rules.upgrades.maxBlastRadius})
        if (!readInt(packet, *value))
            return std::nullopt;
    for (int& weight : rules.upgrades.weights)
        if (!readInt(packet, weight))
            return std::nullopt;
    std::uint64_t seed = 0;
    if (!(packet >> seed))
        return std::nullopt;
    rules.seed = seed;
    return rules;
}

// ---------- message bodies ----------
void writeBody(sf::Packet& packet, const Hello& message)
{
    packet << message.version << message.model;
}

void writeBody(sf::Packet& packet, const Welcome& message)
{
    packet << message.seat;
}

void writeBody(sf::Packet& packet, const Refused& message)
{
    packet << static_cast<std::uint8_t>(message.reason);
}

void writeBody(sf::Packet& packet, const LobbyUpdate& message)
{
    for (const SeatInfo& seat : message.seats)
        packet << seat.taken << seat.model;
    writeGrid(packet, message.map);
}

void writeBody(sf::Packet& packet, const ChangeModel& message)
{
    packet << message.step;
}

void writeBody(sf::Packet& packet, const StartMatch& message)
{
    writeRules(packet, message.rules);
    writeGrid(packet, message.grid);
    packet << static_cast<std::uint8_t>(message.players.size());
    for (const MatchPlayer& player : message.players)
        packet << player.seat << player.model;
    packet << static_cast<std::uint8_t>(message.you);
}

void writeBody(sf::Packet& packet, const Input& message)
{
    packet << message.command.has_value();
    if (message.command)
        writeCommand(packet, *message.command);
}

void writeBody(sf::Packet& packet, const Ticks& message)
{
    packet << static_cast<std::uint16_t>(message.frames.size());
    if (!message.frames.empty())
        packet << message.frames.front().tick; // the next ones follow without gaps
    for (const TickFrame& frame : message.frames)
    {
        packet << static_cast<std::uint8_t>(frame.commands.size());
        for (const auto& [player, command] : frame.commands)
        {
            packet << static_cast<std::uint8_t>(player);
            writeCommand(packet, command);
        }
    }
    packet << message.checksum.has_value();
    if (message.checksum)
        packet << message.checksum->tick << message.checksum->hash;
}

void writeBody(sf::Packet&, const BackToLobby&)
{
}

template <typename T>
constexpr MessageType typeOf()
{
    if constexpr (std::is_same_v<T, Hello>)
        return MessageType::Hello;
    else if constexpr (std::is_same_v<T, Welcome>)
        return MessageType::Welcome;
    else if constexpr (std::is_same_v<T, Refused>)
        return MessageType::Refused;
    else if constexpr (std::is_same_v<T, LobbyUpdate>)
        return MessageType::LobbyUpdate;
    else if constexpr (std::is_same_v<T, ChangeModel>)
        return MessageType::ChangeModel;
    else if constexpr (std::is_same_v<T, StartMatch>)
        return MessageType::StartMatch;
    else if constexpr (std::is_same_v<T, Input>)
        return MessageType::Input;
    else if constexpr (std::is_same_v<T, Ticks>)
        return MessageType::Ticks;
    else
        return MessageType::BackToLobby;
}

std::optional<Message> readBody(MessageType type, sf::Packet& packet)
{
    switch (type)
    {
    case MessageType::Hello:
    {
        Hello message;
        if (!(packet >> message.version >> message.model))
            return std::nullopt;
        return message;
    }
    case MessageType::Welcome:
    {
        Welcome message;
        if (!(packet >> message.seat) || message.seat >= MaxPlayers)
            return std::nullopt;
        return message;
    }
    case MessageType::Refused:
    {
        std::uint8_t reason = 0;
        if (!(packet >> reason) || reason < static_cast<std::uint8_t>(RefusalReason::Full)
            || reason > static_cast<std::uint8_t>(RefusalReason::MatchInProgress))
            return std::nullopt;
        return Refused{static_cast<RefusalReason>(reason)};
    }
    case MessageType::LobbyUpdate:
    {
        LobbyUpdate message;
        for (SeatInfo& seat : message.seats)
            if (!(packet >> seat.taken >> seat.model))
                return std::nullopt;
        std::optional<Grid> map = readGrid(packet);
        if (!map)
            return std::nullopt;
        message.map = std::move(*map);
        return message;
    }
    case MessageType::ChangeModel:
    {
        ChangeModel message;
        if (!(packet >> message.step))
            return std::nullopt;
        return message;
    }
    case MessageType::StartMatch:
    {
        StartMatch message;
        std::optional<GameRules> rules = readRules(packet);
        std::optional<Grid> grid = rules ? readGrid(packet) : std::nullopt;
        std::uint8_t count = 0;
        if (!grid || !(packet >> count) || count == 0 || count > MaxPlayers)
            return std::nullopt;
        message.rules = *rules;
        message.grid = std::move(*grid);
        for (std::uint8_t i = 0; i < count; ++i)
        {
            MatchPlayer player;
            if (!(packet >> player.seat >> player.model) || player.seat >= MaxPlayers)
                return std::nullopt;
            message.players.push_back(player);
        }
        std::uint8_t you = 0;
        if (!(packet >> you) || you >= count)
            return std::nullopt;
        message.you = static_cast<PlayerId>(you);
        return message;
    }
    case MessageType::Input:
    {
        Input message;
        bool hasCommand = false;
        if (!(packet >> hasCommand))
            return std::nullopt;
        if (hasCommand)
        {
            message.command = readCommand(packet);
            if (!message.command)
                return std::nullopt;
        }
        return message;
    }
    case MessageType::Ticks:
    {
        Ticks message;
        std::uint16_t count = 0;
        std::uint64_t tick = 0;
        if (!(packet >> count) || count > MaxFramesPerMessage || (count > 0 && !(packet >> tick)))
            return std::nullopt;
        for (std::uint16_t i = 0; i < count; ++i)
        {
            TickFrame frame;
            frame.tick = tick + i;
            std::uint8_t commands = 0;
            if (!(packet >> commands) || commands > MaxPlayers)
                return std::nullopt;
            for (std::uint8_t k = 0; k < commands; ++k)
            {
                std::uint8_t player = 0;
                if (!(packet >> player) || player >= MaxPlayers)
                    return std::nullopt;
                const std::optional<CharacterCommand> command = readCommand(packet);
                if (!command)
                    return std::nullopt;
                frame.commands.emplace_back(static_cast<PlayerId>(player), *command);
            }
            message.frames.push_back(std::move(frame));
        }
        bool hasChecksum = false;
        if (!(packet >> hasChecksum))
            return std::nullopt;
        if (hasChecksum)
        {
            StateChecksum checksum;
            if (!(packet >> checksum.tick >> checksum.hash))
                return std::nullopt;
            message.checksum = checksum;
        }
        return message;
    }
    case MessageType::BackToLobby:
        return BackToLobby{};
    }
    return std::nullopt;
}

} // namespace

// =========================
// Encoding
// =========================
sf::Packet encode(const Message& message)
{
    sf::Packet packet;
    std::visit(
        [&packet](const auto& body) {
            using T = std::decay_t<decltype(body)>;
            packet << static_cast<std::uint8_t>(typeOf<T>());
            writeBody(packet, body);
        },
        message);
    return packet;
}

std::optional<Message> decode(sf::Packet& packet)
{
    std::uint8_t type = 0;
    if (!(packet >> type) || type < static_cast<std::uint8_t>(MessageType::Hello)
        || type > static_cast<std::uint8_t>(MessageType::BackToLobby))
        return std::nullopt;
    std::optional<Message> message = readBody(static_cast<MessageType>(type), packet);
    if (!message || !packet.endOfPacket())
        return std::nullopt; // trailing bytes: not one of our packets
    return message;
}

sf::Packet encodeBeacon(const Beacon& beacon)
{
    sf::Packet packet;
    packet << BeaconMagic << beacon.version << beacon.gamePort << beacon.hostModel << beacon.seatsTaken
           << beacon.seatCount;
    writeGrid(packet, beacon.map);
    return packet;
}

std::optional<Beacon> decodeBeacon(sf::Packet& packet)
{
    std::uint32_t magic = 0;
    Beacon beacon;
    if (!(packet >> magic) || magic != BeaconMagic
        || !(packet >> beacon.version >> beacon.gamePort >> beacon.hostModel >> beacon.seatsTaken >> beacon.seatCount)
        || beacon.seatCount == 0 || beacon.seatCount > MaxPlayers || beacon.seatsTaken > beacon.seatCount)
        return std::nullopt;
    std::optional<Grid> map = readGrid(packet);
    if (!map || !packet.endOfPacket())
        return std::nullopt;
    beacon.map = std::move(*map);
    return beacon;
}

void writeGrid(sf::Packet& packet, const Grid& grid)
{
    packet << static_cast<std::uint8_t>(grid.getWidth()) << static_cast<std::uint8_t>(grid.getHeight());
    for (int y = 0; y < grid.getHeight(); ++y)
        for (int x = 0; x < grid.getWidth(); ++x)
            packet << static_cast<std::uint8_t>(grid.getTile({x, y}).getType());
    packet << static_cast<std::uint8_t>(grid.getSpawnPoints().size());
    for (const Position spawn : grid.getSpawnPoints())
        packet << static_cast<std::uint8_t>(spawn.x) << static_cast<std::uint8_t>(spawn.y);
    packet << grid.getTheme().substr(0, MaxThemeLength);
}

std::optional<Grid> readGrid(sf::Packet& packet)
{
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    if (!(packet >> width >> height) || width == 0 || height == 0 || width > MaxGridSide || height > MaxGridSide)
        return std::nullopt;
    Grid grid(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            std::uint8_t type = 0;
            if (!(packet >> type) || type > static_cast<std::uint8_t>(TileType::DestructibleBlock))
                return std::nullopt;
            grid.setTile({x, y}, Tile(static_cast<TileType>(type)));
        }
    std::uint8_t spawns = 0;
    if (!(packet >> spawns) || spawns > MaxSpawnPoints)
        return std::nullopt;
    for (std::uint8_t i = 0; i < spawns; ++i)
    {
        std::uint8_t x = 0;
        std::uint8_t y = 0;
        if (!(packet >> x >> y) || x >= width || y >= height)
            return std::nullopt;
        grid.addSpawnPoint({x, y});
    }
    std::string theme;
    if (!(packet >> theme) || theme.size() > MaxThemeLength)
        return std::nullopt;
    grid.setTheme(std::move(theme));
    return grid;
}

// =========================
// Lockstep helpers
// =========================
std::vector<TickFrame> makeTickFrames(std::uint64_t firstTick, int count, const std::vector<ExecutedCommand>& executed)
{
    std::vector<TickFrame> frames(static_cast<std::size_t>(std::max(count, 0)));
    for (std::size_t i = 0; i < frames.size(); ++i)
        frames[i].tick = firstTick + i;
    for (const ExecutedCommand& command : executed)
        if (command.tick >= firstTick && command.tick < firstTick + frames.size())
            frames[static_cast<std::size_t>(command.tick - firstTick)].commands.emplace_back(command.player,
                                                                                              command.command);
    return frames;
}

bool applyTickFrame(GameEngine& engine, const TickFrame& frame)
{
    if (engine.getTick() != frame.tick)
        return false;
    for (const auto& [player, command] : frame.commands)
        engine.submitCommand(player, command);
    engine.tick();
    return true;
}

} // namespace Bomberman

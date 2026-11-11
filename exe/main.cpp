#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/Network/IpAddress.hpp>

#include "character_model.h"
#include "client_session.h"
#include "discovery.h"
#include "discovery_renderer.h"
#include "game_engine.h"
#include "grid.h"
#include "grid_loader.h"
#include "host_session.h"
#include "keyboard_controller.h"
#include "lobby_renderer.h"
#include "match.h"
#include "match_setup.h"
#include "match_setup_renderer.h"
#include "protocol.h"

namespace
{

using namespace Bomberman;

constexpr int LevelCount = 4;

// $BOMBERMAN_ROOT/data/grid/level_0N.json (or ./data when BOMBERMAN_ROOT is
// not defined), N in [1, LevelCount].
std::filesystem::path levelPath(int index)
{
    const char* bombermanRoot = std::getenv("BOMBERMAN_ROOT");
    return std::filesystem::path(bombermanRoot ? bombermanRoot : ".") / "data" / "grid"
         / ("level_0" + std::to_string(index) + ".json");
}

// A map the menu offers.
struct MenuMap
{
    std::filesystem::path path;
    Grid grid;
};

// The level given on the command line (if any) first, then the shipped
// ones. Levels that cannot be played are reported and left out.
std::vector<MenuMap> loadMenuMaps(const std::optional<std::filesystem::path>& extra)
{
    std::vector<std::filesystem::path> paths;
    if (extra)
        paths.push_back(*extra);
    for (int i = 1; i <= LevelCount; ++i)
        paths.push_back(levelPath(i));

    std::vector<MenuMap> maps;
    for (const std::filesystem::path& path : paths)
    {
        try
        {
            Grid grid = GridLoader::loadFromFile(path);
            if (grid.getSpawnPoints().empty())
            {
                std::cerr << path << " has no spawn point" << std::endl;
                continue;
            }
            std::cout << "Loaded " << path << " (" << grid.getWidth() << "x" << grid.getHeight() << ", theme \""
                      << grid.getTheme() << "\", " << grid.getSpawnPoints().size() << " spawn points)" << std::endl;
            maps.push_back(MenuMap{path, std::move(grid)});
        }
        catch (const GridLoadError& error)
        {
            std::cerr << "Failed to load grid: " << error.what() << std::endl;
        }
    }
    return maps;
}

std::uint64_t freshSeed()
{
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32) ^ static_cast<std::uint64_t>(device());
}

// Command line: [level.json] [--join <address>[:<port>]].
struct Options
{
    std::optional<std::filesystem::path> level;
    std::optional<std::string> join;
};

Options parseOptions(int argc, char* argv[])
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        if (arg == "--join" && i + 1 < argc)
            options.join = argv[++i];
        else
            options.level = std::filesystem::path(argv[i]);
    }
    return options;
}

// "192.168.1.20" or "192.168.1.20:53790" (or a host name).
std::optional<std::pair<sf::IpAddress, unsigned short>> parseAddress(const std::string& text)
{
    std::string host = text;
    unsigned short port = DefaultGamePort;
    if (const auto colon = text.rfind(':'); colon != std::string::npos)
    {
        host = text.substr(0, colon);
        try
        {
            const int value = std::stoi(text.substr(colon + 1));
            if (value <= 0 || value > 65535)
                return std::nullopt;
            port = static_cast<unsigned short>(value);
        }
        catch (const std::exception&)
        {
            return std::nullopt;
        }
    }
    const std::optional<sf::IpAddress> address = sf::IpAddress::resolve(host);
    if (!address)
        return std::nullopt;
    return std::make_pair(*address, port);
}

const char* describe(RefusalReason reason)
{
    switch (reason)
    {
    case RefusalReason::Full:
        return "the crew is full";
    case RefusalReason::VersionMismatch:
        return "the host runs another version of the game";
    case RefusalReason::MatchInProgress:
        return "a match is in progress";
    }
    return "refused";
}

// The screens of the game.
enum class Screen
{
    Menu,   // launch menu (MatchSetupRenderer)
    Search, // looking for a network game (DiscoveryRenderer)
    Lobby,  // waiting room of a network game, as host or client (LobbyRenderer)
    Match
};

} // namespace

int main(int argc, char* argv[])
{
    std::cout << "Start Bomberman" << std::endl;
    if (!std::getenv("BOMBERMAN_ROOT"))
        std::cout << "BOMBERMAN_ROOT is not defined, looking for data in the current directory\n";

    const Options options = parseOptions(argc, argv);

    // Fullscreen at the desktop resolution.
    const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
    const sf::Vector2u screenSize = desktopMode.size;

    // Fail early if there is nothing to play on.
    const std::vector<MenuMap> maps = loadMenuMaps(options.level);
    if (maps.empty())
        return EXIT_FAILURE;

    std::vector<Grid> menuGrids;
    for (const MenuMap& map : maps)
        menuGrids.push_back(map.grid);

    // Launch on the setup menu: Z/S or the up/down arrows pick a row, Q/D
    // or the left/right arrows change it, Enter/Space starts the match (or
    // opens the network screens); Tab in game comes back to it.
    MatchSetup setup;
    setup.mapCount = static_cast<int>(maps.size());
    MatchSetupRenderer menu(menuGrids);
    Screen screen = Screen::Menu;
    std::unique_ptr<Match> match;

    // Network game (see doc/network.md).
    std::unique_ptr<HostSession> host;
    std::unique_ptr<ClientSession> client;
    std::unique_ptr<DiscoveryListener> discovery;
    DiscoveryRenderer searchScreen;
    int searchSelection = 0;
    LobbyRenderer lobbyScreen;
    bool lobbyMapKnown = false; // a client learns the map with the first LobbyUpdate

    // The key that started the match (Space also drops bombs) must be
    // released before the keyboard drives the player.
    bool waitForRelease = false;
    const KeyboardController keyboard;

    sf::ContextSettings windowSettings;
    windowSettings.antiAliasingLevel = 8; // smooths the character's rounded/angled shapes

    sf::RenderWindow window(desktopMode, "Bomberman", sf::Style::None, sf::State::Fullscreen, windowSettings);
    window.setFramerateLimit(60);
    const sf::View menuView = MatchSetupRenderer::viewFor(screenSize);
    window.setView(menuView);

    constexpr std::array<sf::Keyboard::Key, LevelCount> levelKeys = {sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2,
                                                                      sf::Keyboard::Key::Num3, sf::Keyboard::Key::Num4};

    auto toMenu = [&]() {
        match.reset();
        host.reset();
        client.reset();
        discovery.reset();
        screen = Screen::Menu;
        window.setView(menuView);
    };
    auto toLobby = [&]() {
        match.reset();
        screen = Screen::Lobby;
        window.setView(menuView);
    };
    auto enterMatch = [&](std::unique_ptr<Match> started) {
        match = std::move(started);
        screen = Screen::Match;
        waitForRelease = true;
    };
    auto playSolo = [&]() {
        enterMatch(startSoloMatch(maps[static_cast<std::size_t>(setup.map)].grid, setup, freshSeed(), screenSize));
    };
    auto hostGame = [&]() {
        const Grid& grid = maps[static_cast<std::size_t>(setup.map)].grid;
        host = std::make_unique<HostSession>(grid, CharacterModelCount, indexOfModel(setup.pirate));
        if (!host->isListening())
        {
            std::cerr << "Cannot host: no TCP port available" << std::endl;
            host.reset();
            return;
        }
        std::cout << "Hosting on port " << host->getPort() << " (other players: --join <this computer's address>"
                  << (host->getPort() != DefaultGamePort ? ":" + std::to_string(host->getPort()) : "") << ")"
                  << std::endl;
        lobbyScreen.setMap(grid);
        toLobby();
    };
    auto joinGame = [&](sf::IpAddress address, unsigned short port) {
        std::cout << "Joining " << address.toString() << ":" << port << std::endl;
        client = std::make_unique<ClientSession>(address, port, indexOfModel(setup.pirate));
        if (client->getState() == ClientSession::State::Disconnected)
        {
            std::cerr << "Cannot reach " << address.toString() << ":" << port << std::endl;
            client.reset();
            return;
        }
        discovery.reset();
        lobbyMapKnown = false;
        toLobby();
    };

    if (options.join)
    {
        if (const auto address = parseAddress(*options.join))
            joinGame(address->first, address->second);
        else
            std::cerr << "Invalid address: " << *options.join << std::endl;
    }

    sf::Clock clock;
    while (window.isOpen())
    {
        const float deltaSeconds = clock.restart().asSeconds();

        // =========================
        // Input
        // =========================
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
            if (!keyPressed)
                continue;
            using Key = sf::Keyboard::Key;
            const Key key = keyPressed->code;
            const bool left = key == Key::Q || key == Key::Left;
            const bool right = key == Key::D || key == Key::Right;
            const bool confirm = key == Key::Enter || key == Key::Space;

            switch (screen)
            {
            case Screen::Menu:
                if (key == Key::Escape)
                    window.close();
                else if (key == Key::Z || key == Key::Up)
                    setup.moveFocus(-1);
                else if (key == Key::S || key == Key::Down)
                    setup.moveFocus(1);
                else if (left || right)
                    setup.change(left ? -1 : 1);
                else if (confirm && setup.canStart())
                {
                    if (setup.mode == GameMode::VersusComputer)
                        playSolo();
                    else if (setup.mode == GameMode::Host)
                        hostGame();
                    else
                    {
                        discovery = std::make_unique<DiscoveryListener>();
                        if (!discovery->isListening())
                            std::cerr << "Cannot listen for games on UDP port " << DiscoveryPort
                                      << " (another Bomberman on this computer?): use --join <address>" << std::endl;
                        searchSelection = 0;
                        screen = Screen::Search;
                    }
                }
                break;

            case Screen::Search:
            {
                const auto& hosts = discovery->getHosts();
                if (key == Key::Escape)
                    toMenu();
                else if ((left || right) && !hosts.empty())
                    searchSelection = MatchSetup::wrap(searchSelection, left ? -1 : 1, static_cast<int>(hosts.size()));
                else if (confirm && !hosts.empty())
                {
                    const DiscoveredHost& chosen =
                        hosts[static_cast<std::size_t>(std::clamp(searchSelection, 0, static_cast<int>(hosts.size()) - 1))];
                    joinGame(chosen.address, chosen.beacon.gamePort);
                }
                break;
            }

            case Screen::Lobby:
                if (key == Key::Escape)
                    toMenu(); // leaves the game (a host closes it for everyone)
                else if ((left || right) && host)
                    (void)host->changeHostModel(left ? -1 : 1);
                else if ((left || right) && client)
                    client->changeModel(left ? -1 : 1);
                else if (confirm && host && host->getLobby().canStart())
                {
                    GameRules rules;
                    rules.seed = freshSeed();
                    if (const std::optional<StartMatch> start = host->startMatch(rules))
                        enterMatch(startNetworkMatch(*start, MatchRole::Host, screenSize));
                }
                break;

            case Screen::Match:
                if (key == Key::Escape)
                    window.close();
                else if (key == Key::Tab)
                {
                    // Back to the menu (solo), to the waiting room with
                    // everyone (host), out of the game (client).
                    if (match->role == MatchRole::Host)
                    {
                        host->returnToLobby(true);
                        toLobby();
                    }
                    else
                        toMenu();
                }
                else if (match->role == MatchRole::Solo)
                {
                    // 1-4: restart on the matching shipped map, same setup.
                    const auto levelKey = std::find(levelKeys.begin(), levelKeys.end(), key);
                    if (levelKey == levelKeys.end())
                        break;
                    const std::filesystem::path wanted = levelPath(static_cast<int>(levelKey - levelKeys.begin()) + 1);
                    const auto map = std::find_if(maps.begin(), maps.end(),
                                                  [&](const MenuMap& candidate) { return candidate.path == wanted; });
                    if (map != maps.end())
                    {
                        setup.map = static_cast<int>(map - maps.begin());
                        playSolo();
                    }
                }
                break;
            }
            if (!window.isOpen())
                break;
        }
        if (!window.isOpen())
            break;

        // =========================
        // Network
        // =========================
        if (host)
            host->update(deltaSeconds);
        if (client)
        {
            client->update();
            if (client->getState() == ClientSession::State::Refused
                || client->getState() == ClientSession::State::Disconnected)
            {
                if (const auto reason = client->getRefusal())
                    std::cerr << "The host refused us: " << describe(*reason) << std::endl;
                else
                    std::cerr << "Lost the connection to the host" << std::endl;
                toMenu();
            }
            else if (std::optional<StartMatch> start = client->takeStart())
                enterMatch(startNetworkMatch(*start, MatchRole::Client, screenSize));
            else if (client->takeBackToLobby() && screen == Screen::Match && !match->referee.isOver())
                toLobby(); // the host abandoned the match
        }
        if (discovery)
            discovery->update(deltaSeconds);

        // =========================
        // Screens without a match
        // =========================
        if (screen != Screen::Match)
        {
            window.clear(sf::Color::Black);
            if (screen == Screen::Menu)
            {
                menu.update(setup, deltaSeconds);
                window.draw(menu);
            }
            else if (screen == Screen::Search)
            {
                std::vector<DiscoveryCard> cards;
                for (const DiscoveredHost& heard : discovery->getHosts())
                    cards.push_back(DiscoveryCard{heard.address.toString() + ":" + std::to_string(heard.beacon.gamePort),
                                                  modelAt(heard.beacon.hostModel), heard.beacon.seatsTaken,
                                                  heard.beacon.seatCount, heard.beacon.map});
                searchSelection = std::clamp(searchSelection, 0, std::max(static_cast<int>(cards.size()) - 1, 0));
                searchScreen.update(cards, searchSelection, deltaSeconds);
                window.draw(searchScreen);
            }
            else
            {
                LobbyView view;
                if (host)
                {
                    const Lobby& lobby = host->getLobby();
                    for (int seat = 0; seat < lobby.getSeatCount(); ++seat)
                        if (const std::optional<int> model = lobby.getModel(seat))
                            view.seats[static_cast<std::size_t>(seat)] = modelAt(*model);
                    view.localSeat = 0;
                    view.isHost = true;
                    view.canStart = lobby.canStart();
                }
                else if (client)
                {
                    if (const std::optional<LobbyUpdate>& lobby = client->getLobby())
                    {
                        for (std::size_t seat = 0; seat < lobby->seats.size(); ++seat)
                            if (lobby->seats[seat].taken)
                                view.seats[seat] = modelAt(lobby->seats[seat].model);
                        if (!lobbyMapKnown)
                        {
                            lobbyScreen.setMap(lobby->map);
                            lobbyMapKnown = true;
                        }
                    }
                    view.localSeat = client->getSeat().value_or(-1);
                    view.isHost = false;
                }
                lobbyScreen.update(view, deltaSeconds);
                window.draw(lobbyScreen);
            }
            window.display();
            continue;
        }

        // =========================
        // Match
        // =========================
        GameEngine& engine = match->engine;

        // Input only produces a command; the engine decides at its next tick
        // whether the player is ready to execute it (actions are paced by
        // GameRules delays). A move is immediate (holding the key keeps
        // walking); Space drops a bomb where the player stands.
        if (waitForRelease && !keyboard.poll())
            waitForRelease = false;
        const std::optional<CharacterCommand> command = waitForRelease ? std::nullopt : keyboard.poll();

        switch (match->role)
        {
        case MatchRole::Solo:
            // The bots are asked by the engine itself, at every tick where
            // they are ready. The referee speeds the match up once the
            // player is dead and stops it once it is over.
            if (command)
                engine.submitCommand(match->player, *command);
            (void)engine.advance(match->referee.gameSeconds(deltaSeconds));
            break;

        case MatchRole::Host:
        {
            // The clients' keys are submitted like local ones, then the
            // commands every player executed are relayed tick by tick.
            if (command)
                engine.submitCommand(match->player, *command);
            for (PlayerId id = 0; id < engine.getPlayerCount(); ++id)
                if (id != match->player)
                    if (const std::optional<CharacterCommand> remote = host->takeCommand(id))
                        engine.submitCommand(id, *remote);
            const std::uint64_t firstTick = engine.getTick();
            const int ran = engine.advance(match->referee.gameSeconds(deltaSeconds));
            if (ran > 0)
            {
                Ticks ticks;
                ticks.frames = makeTickFrames(firstTick, ran, engine.takeExecutedCommands());
                if (firstTick / ChecksumInterval != engine.getTick() / ChecksumInterval)
                    ticks.checksum = StateChecksum{engine.getTick(), engine.stateHash()};
                host->sendTicks(ticks);
            }
            break;
        }

        case MatchRole::Client:
            // The keyboard goes to the host; the board only moves with the
            // host's ticks, replayed at the tick rate.
            client->sendInput(command);
            for (const Ticks& ticks : client->takeTicks())
                match->playout.push(ticks);
            for (const TickFrame& frame : match->playout.take(deltaSeconds))
            {
                if (!applyTickFrame(engine, frame))
                    continue; // cannot happen with an ordered TCP stream
                const std::optional<std::uint64_t> expected = match->playout.takeChecksum(engine.getTick());
                if (expected && *expected != engine.stateHash() && !match->desynchronised)
                {
                    match->desynchronised = true;
                    std::cerr << "Desynchronised from the host at tick " << engine.getTick()
                              << " (see doc/network.md)" << std::endl;
                }
            }
            break;
        }

        presentFrame(*match, engine.takeEvents(), deltaSeconds);

        window.clear(sf::Color::Black);
        const bool finished = drawMatch(window, *match, screenSize, deltaSeconds);
        window.display();

        if (finished)
        {
            // Network players meet again in the waiting room.
            if (match->role == MatchRole::Host)
            {
                host->returnToLobby(false);
                toLobby();
            }
            else if (match->role == MatchRole::Client)
                toLobby();
            else
                toMenu();
        }
    }

    return EXIT_SUCCESS;
}

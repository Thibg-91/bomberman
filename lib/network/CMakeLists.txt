find_package(SFML COMPONENTS System Network REQUIRED)

# Multiplayer over the local network (see doc/network.md): protocol,
# waiting room, host/client sessions, discovery. Depends on the engine but
# never on render (no window needed, testable headless over loopback).
bomberman_add_library(network
    LINK_LIBRARIES
        Bomberman::grid
        Bomberman::character
        Bomberman::engine
        SFML::Network
)

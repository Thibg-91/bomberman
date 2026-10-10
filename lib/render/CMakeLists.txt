find_package(SFML COMPONENTS System Window Graphics REQUIRED)

bomberman_add_library(render
    LINK_LIBRARIES
        Bomberman::grid
        Bomberman::character
        SFML::Graphics
)

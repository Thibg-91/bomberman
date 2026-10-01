find_package(nlohmann_json REQUIRED)

bomberman_add_library(grid
    LINK_LIBRARIES
        nlohmann_json::nlohmann_json
)

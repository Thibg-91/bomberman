# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Minimalist Bomberman in C++17 (SFML 3, nlohmann_json, GoogleTest). Built with CMake driven by Conan, tasks wrapped in [Taskfile.yaml](Taskfile.yaml). Docs (in French) live in `doc/` (`grid.md`, `character.md`, `sfml.md`); feature specs/prompts live in `prompt/`.

## Commands

Run from the repo root. `BUILD_TYPE` defaults to `Release` (override with `task build BUILD_TYPE=Debug`; the other tasks must use the same value).

- `task install` — `conan install` (dependencies; needed before the first build)
- `task build` — `conan build .`: configures, compiles **and runs the tests** (`cmake.test()` in `conanfile.py`). Skip tests with `conan build . -c tools.build:skip_test=True`
- `task test` — `ctest --test-dir build/Release --output-on-failure`
- Single test: `ctest --test-dir build/Release -R <regex> --output-on-failure` (or run `build/Release/bin/grid_tests --gtest_filter=Suite.Case`)
- `task coverage` — GCC/Clang only (`BOMBERMAN_COVERAGE` fails on MSVC); gcovr report in `build/Release/coverage`, line/function/branch percentages printed at the end
- `task conan-all` = install + build + `export-pkg`; `task consume` installs the packaged result with `VirtualRunEnv` into `env/`

Running the game: the executable looks for `$BOMBERMAN_ROOT/data/grid/level_01.json` (or `./data/...` if unset); a level path can be passed as first argument. Binaries and DLLs all land in `build/Release/bin`.

## Architecture

- `lib/<name>/` — one **shared library** per folder, sources in `src/`, target `Bomberman::<name>`. Currently `grid` (logic: `Position`, `Tile`, `Grid`, `GridLoader`; no SFML dependency, so it is testable without a window), `character` (logic: `Character`, `CharacterCommand`, `Direction`; depends on `grid` only) and `render` (SFML: `GridRenderer`, `CharacterRenderer`, `KeyboardController`; depends on `grid` + `character`). `lib/cpp_template` is a skeleton that is not wired into CMake.
- `exe/` — the `bomberman` executable. `tests/<name>/` mirrors `lib/<name>` (one GoogleTest executable per lib, e.g. `grid_tests`).
- `data/` — level JSON files (format described in `doc/grid.md`); packaged by Conan and exposed through `BOMBERMAN_ROOT`. Tests get the source `data/` via the `BOMBERMAN_DATA_DIR` compile definition.
- Character: `Character` moves cell by cell and is driven **only** through `CharacterCommand` (`move(dir)` / `breakBlock()`) via `Character::execute(command, grid)`. Keyboard (`KeyboardController`, default z/q/s/d + Enter), future AI and future network input must all just produce commands; never put input or SFML in `character`. Time is injected with `update(dt)`: a move changes the logical position immediately, then the character is busy for `moveDuration` (new commands ignored) while `CharacterRenderer` interpolates `getPreviousPosition()` → `getPosition()` with `getMoveProgress()`. A blocked move still turns the character; `breakBlock()` destroys the faced destructible tile and returns true, in which case the caller must call `GridRenderer::update(grid)` (back/front sync). `exe/main.cpp` spawns the player on the first spawn point. AI and multiplayer are not implemented yet.
- Design intent: tiles are static cell content only; players/bombs/bonuses will be separate entities positioned via `Position` with their own renderers drawn over `GridRenderer`.

### CMake conventions

Targets are created with the macros in `cmake/` (`bomberman_add_library`, `bomberman_add_header_only_library`, `bomberman_add_executable`, `bomberman_add_test`), not raw `add_library`. Sources are **globbed** (`CONFIGURE_DEPENDS`) from `src/` (or next to the CMakeLists for exe/tests), so adding a file needs no CMake edit; use `EXCLUDE` to leave one out. A new lib/test folder needs an `add_subdirectory` in the parent `CMakeLists.txt`.

### Windows/DLL gotchas

- Libraries are shared by default and `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` is on, so no `__declspec` markup is needed.
- SFML **must** stay `shared` (`conanfile.py` default option): `render.dll` exports all symbols, a static SFML would be duplicated in each consumer.
- Tests use `gtest_discover_tests(... PRE_TEST)` so building never requires the DLLs to be loadable.
- `Taskfile.yaml` must stay valid UTF-8 (a Latin-1 accent makes `task` refuse to parse it).
- Warnings are on (`/W4` or `-Wall -Wextra`).

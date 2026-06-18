# Build / type-check / test commands

This project is configured for the `sims` C++20 codebase. Use these commands
verbatim when working on this repo.

## Build

Configure once (out-of-source build):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

Compile the app:

    cmake --build build -j

## Run

    ./build/sims

Requires a display (X11/Wayland). On headless/CI the binary is only compiled,
not executed. If `SDL_CreateWindow` fails because there is no display, install
`xvfb` and run:

    xvfb-run -a ./build/sims

## Clean rebuild

    cmake --build build -j --clean-first

## Dependencies (system)

    sudo pacman -S sdl2 assimp gcc cmake git  # Arch example

Header-only libraries (glm, entt, nlohmann/json, Catch2) and Dear ImGui + GLAD
are pulled via CMake FetchContent on first configure. Network access required
on the first `cmake -S . -B build` run.

## Conventions

- C++20, `-Wall -Wextra -Wpedantic`.
- No `using namespace` in headers.
- Header guards via `#pragma once`.
- All engine code under `src/` and namespace `sims`.
- Do NOT add comments to source unless explicitly requested.
- Never commit changes unless explicitly asked.

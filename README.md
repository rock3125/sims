# sims

A *Sims*-like life-simulation game prototype, written in modern C++ (C++20) on
SDL2 + OpenGL 3.3 Core. Data-driven content (JSON), entity-component-system
(entt) architecture, and a fixed-timestep simulation.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

## Stack

- **Window/Input/Audio:** SDL2
- **Graphics:** OpenGL 3.3 Core + GLAD
- **Math:** glm
- **ECS:** entt
- **JSON:** nlohmann/json
- **UI:** Dear ImGui (debug + future pie-menu / HUD)
- **Build:** CMake 3.20+, C++20
- **Tests:** Catch2 (enabled in later phases)

See `AGENTS.md` for the canonical build/test commands.

## Project layout

```
src/
├── main.cpp            # entry point
├── app/                # Application: window, GL context, main loop
├── render/             # (Phase 1+) Shader, Mesh, Camera, Renderer
├── ecs/                # entt type aliases, component structs
├── sim/                # world, motives, ai, pathfinding, interactions, time
├── content/            # JSON loaders, asset registry
├── input/              # event mapping, picking
└── ui/                 # ImGui panels, pie-menu, HUD
assets/
├── models/ textures/ shaders/
└── definitions/        # objects.json, needs.json, lots.json
tests/                  # (Phase 1+) Catch2 unit tests
```

## Roadmap

- **Phase 0 (this):** SDL2 window + OpenGL 3.3 context, ImGui debug overlay,
  fixed-step main loop. Smoke gate: window opens, ImGui renders, Esc quits.
- **Phase 1:** Renderer (glTF via Assimp, textured meshes, orbit camera).
- **Phase 2:** Tile grid world + walls.
- **Phase 3:** Sim avatar + walk animation.
- **Phase 4:** A* pathfinding.
- **Phase 5:** Motives (hunger/energy/bladder/fun/social) + SimClock.
- **Phase 6:** Interactions (JSON-driven, pie-menu, ActionQueue).
- **Phase 7:** Utility AI autonomy.
- **Phase 8:** Save/load, build mode, audio polish.

## Controls

- **Esc** — quit
- **F1** — toggle ImGui demo window

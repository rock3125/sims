#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <deque>
#include <string>

namespace sims {

// World-space transform of an entity. Yaw is around the +Y axis; sim
// objects live on the y=0 plane in Phase 2.
struct Transform {
    glm::vec3 position{0.0f};
    float yaw_deg = 0.0f;
    glm::vec3 scale{1.0f};
};

// Marks an entity as occupying one or more grid tiles. footprint_w/footprint_d
// are in tiles and rotate by yaw. Phase 2 keeps it 1x1 only; richer footprints
// arrive when objects are introduced in Phase 6.
struct TileOccupant {
    int tile_x = 0;
    int tile_z = 0;
    int footprint_w = 1;
    int footprint_d = 1;
};

// A wall segment on a tile edge. Edges are N/S/E/W per tile. Two adjacent
// tiles share an edge — we canonicalize by storing the lower-coordinate tile
// and the edge direction facing +x or +z (i.e. E and N). Phase 2 renders
// walls as thin 0.1m-wide, 2.7m-tall boxes.
struct Wall {
    int tile_x = 0;
    int tile_z = 0;
    enum class Side : std::uint8_t { East, North } side;
};

// Renderable tag: which mesh + base color to use. Resolves to a small set of
// built-in primitives in Phase 2 (cube / wall / plane). Later phases replace
// this with an asset handle pointing into a Model registry.
struct Renderable {
    enum class Shape : std::uint8_t { Cube, Wall, Plane } shape = Shape::Cube;
    glm::vec3 color{1.0f};
};

// A Sim agent. Phase 3: single Sim, click-to-walk, no needs yet.
struct Sim {
    std::string name = "Sim";
    float facing_deg = 0.0f;     // yaw around +Y; 0 = facing +z
    float walk_phase = 0.0f;     // advances while moving; drives leg/arm swing
    bool moving = false;
};

// Movement state: a queue of world-space waypoints (tile centers) the Sim
// walks through in order. Phase 4: waypoints are produced by the A* pathfinder.
// `target` is the current waypoint being approached (or the last reached
// position when idle). `final_target` is the user-requested destination.
struct Movement {
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 final_target{0.0f, 0.0f, 0.0f};
    float speed = 2.0f;          // meters per second
    bool has_target = false;
    std::deque<glm::vec3> waypoints; // remaining waypoints after `target`
};

} // namespace sims

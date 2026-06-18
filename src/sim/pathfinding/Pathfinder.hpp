#pragma once

#include "sim/world/Coord.hpp"
#include "sim/world/TileGrid.hpp"

#include <glm/glm.hpp>

#include <array>
#include <optional>
#include <vector>

namespace sims {
namespace pathfinding {

// A tile-coordinate path through the lot. waypoints[0] is the start tile;
// the last entry is the goal tile. Each consecutive pair is orthogonally
// adjacent and not separated by a wall.
struct Path {
    std::vector<glm::ivec2> waypoints;
    bool valid = false;

    bool empty() const { return waypoints.size() < 2; }
    std::size_t size() const { return waypoints.size(); }
};

// A* pathfinder over a TileGrid. Movement is 4-connected (orthogonal
// neighbors only); an edge is impassable when the TileGrid reports a wall on
// the shared edge between two tiles. Tiles outside the lot are impassable.
//
// Costs are uniform (1 per step), so the open set is a simple binary heap
// keyed by g + h, with h = Manhattan distance to the goal.
class Pathfinder {
public:
    explicit Pathfinder(const world::TileGrid& grid) : grid_(grid) {}

    // Find a path from `start` to `goal` (tile coords). Returns a Path whose
    // `valid` is false when no route exists or either endpoint is out of
    // bounds. If start == goal, returns a single-tile "path" (valid=true,
    // empty()=true).
    Path find(const glm::ivec2& start, const glm::ivec2& goal) const;

    // True if `from` and `to` (orthogonal neighbors) have no wall between
    // them and both tiles are in bounds. Diagonal pairs return false.
    bool passable(int from_x, int from_z, int to_x, int to_z) const;

private:
    const world::TileGrid& grid_;

    struct Neighbor {
        int dx;
        int dz;
    };

    static constexpr std::array<Neighbor, 4> kNeighbors = {{
        {+1,  0},
        {-1,  0},
        { 0, +1},
        { 0, -1},
    }};
};

} // namespace pathfinding
} // namespace sims

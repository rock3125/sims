#pragma once

#include "sim/world/Coord.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace sims {
namespace world {

// TileGrid owns the 2D lot: which tiles exist, occupancy, and wall edges.
// Walls are stored per-tile as a bitmask of side flags (E and N only — the
// W edge of tile (x,z) is the E edge of tile (x-1,z), etc.).
//
// Wall storage is sparse via a hash set of edge keys to keep memory low for
// large lots with few walls.
class TileGrid {
public:
    TileGrid() = default;
    explicit TileGrid(int w, int d) : coord_{w, d} {}

    const Coord& coord() const { return coord_; }
    int width() const { return coord_.lot_w; }
    int depth() const { return coord_.lot_d; }
    void resize(int w, int d) { coord_.lot_w = w; coord_.lot_d = d; walls_.clear(); }

    // Wall edges — canonical form: a wall is identified by (tile_x, tile_z, side)
    // where side is East (+x) or North (+z). Edges on the lot boundary are valid.
    enum class Side : std::uint8_t { East, North };

    struct Edge {
        int tile_x;
        int tile_z;
        Side side;
        bool operator==(const Edge&) const = default;
    };

    struct EdgeHash {
        std::size_t operator()(const Edge& e) const noexcept {
            // Pack (x, z, side) into 64 bits; x and z are small ints.
            std::uint64_t k = (static_cast<std::uint64_t>(e.tile_x) & 0xFFFFF)
                | (static_cast<std::uint64_t>(e.tile_z & 0xFFFFF) << 20)
                | (static_cast<std::uint64_t>(e.side) << 41);
            return std::hash<std::uint64_t>{}(k);
        }
    };

    using WallSet = std::unordered_set<Edge, EdgeHash>;

    bool has_wall(const Edge& e) const { return walls_.contains(e); }
    bool has_wall(int tx, int tz, Side s) const { return has_wall({tx, tz, s}); }

    void add_wall(const Edge& e) {
        if (in_bounds_edge(e)) walls_.insert(e);
    }
    void add_wall(int tx, int tz, Side s) { add_wall({tx, tz, s}); }

    void remove_wall(const Edge& e) { walls_.erase(e); }
    void remove_wall(int tx, int tz, Side s) { remove_wall({tx, tz, s}); }

    const WallSet& walls() const { return walls_; }
    std::size_t wall_count() const { return walls_.size(); }

    // Is a tile in bounds of the lot?
    bool in_bounds_tile(int tx, int tz) const {
        return tx >= 0 && tz >= 0 && tx < coord_.lot_w && tz < coord_.lot_d;
    }

    // Is a wall edge valid? E edges live on tile (tx, tz) with tx in [0, W],
    // tz in [0, D-1]; N edges live on tile (tx, tz) with tx in [0, W-1], tz in
    // [0, D]. (Edges on the +x/+z boundary of the lot are buildable.)
    bool in_bounds_edge(const Edge& e) const {
        if (e.side == Side::East) {
            return e.tile_x >= 0 && e.tile_x <= coord_.lot_w
                && e.tile_z >= 0 && e.tile_z < coord_.lot_d;
        }
        return e.tile_x >= 0 && e.tile_x < coord_.lot_w
            && e.tile_z >= 0 && e.tile_z <= coord_.lot_d;
    }

    // World-space endpoints of a wall edge (at y=0). Used by the renderer.
    void edge_endpoints(const Edge& e, glm::vec3& a, glm::vec3& b) const {
        // Tile (tx, tz) center; East edge spans z in [tz, tz+1], x = tile_min_x + 1.
        float x0 = static_cast<float>(e.tile_x) - coord_.lot_w * 0.5f;
        float z0 = static_cast<float>(e.tile_z) - coord_.lot_d * 0.5f;
        if (e.side == Side::East) {
            a = { x0 + 1.0f, 0.0f, z0 };
            b = { x0 + 1.0f, 0.0f, z0 + 1.0f };
        } else {
            a = { x0,         0.0f, z0 + 1.0f };
            b = { x0 + 1.0f, 0.0f, z0 + 1.0f };
        }
    }

private:
    Coord coord_{16, 16};
    WallSet walls_;
};

} // namespace world
} // namespace sims

#pragma once

#include <glm/glm.hpp>

namespace sims {
namespace world {

// Grid coordinate conventions:
//   - Tile (tx, tz) occupies the square [tx, tx+1) x [tz, tz+1) in tile space.
//   - Tile (0,0) is at the -x/-z corner of the lot; lot is centered on origin.
//   - World position of tile center: (tx - W/2 + 0.5, 0, tz - D/2 + 0.5).
//   - All units are meters; each tile is 1m x 1m.

struct Coord {
    // Lot dimensions (in tiles). Lot is centered on the world origin.
    int lot_w = 16;
    int lot_d = 16;

    // Tile center -> world position (y=0 plane).
    glm::vec3 tile_to_world(int tx, int tz) const {
        return {
            static_cast<float>(tx) - lot_w * 0.5f + 0.5f,
            0.0f,
            static_cast<float>(tz) - lot_d * 0.5f + 0.5f,
        };
    }

    // World position (y=0 plane) -> nearest tile coords. Returns false if
    // the point is outside the lot.
    bool world_to_tile(const glm::vec3& p, int& out_tx, int& out_tz) const {
        float fx = p.x + lot_w * 0.5f;
        float fz = p.z + lot_d * 0.5f;
        if (fx < 0.0f || fz < 0.0f || fx >= lot_w || fz >= lot_d) return false;
        out_tx = static_cast<int>(fx);
        out_tz = static_cast<int>(fz);
        return true;
    }
};

} // namespace world
} // namespace sims

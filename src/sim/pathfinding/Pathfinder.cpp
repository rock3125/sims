#include "sim/pathfinding/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <vector>

namespace sims {
namespace pathfinding {

namespace {

struct Key {
    int x;
    int z;
    bool operator==(const Key&) const = default;
};

struct KeyHash {
    std::size_t operator()(const Key& k) const noexcept {
        return static_cast<std::size_t>(static_cast<std::uint32_t>(k.x))
             | (static_cast<std::size_t>(static_cast<std::uint32_t>(k.z)) << 32);
    }
};

struct Node {
    Key key;
    int f; // g + h
};

struct NodeCmp {
    bool operator()(const Node& a, const Node& b) const noexcept { return a.f > b.f; }
};

} // namespace

bool Pathfinder::passable(int from_x, int from_z, int to_x, int to_z) const {
    if (!grid_.in_bounds_tile(from_x, from_z) || !grid_.in_bounds_tile(to_x, to_z)) {
        return false;
    }
    int dx = to_x - from_x;
    int dz = to_z - from_z;
    if (std::abs(dx) + std::abs(dz) != 1) return false;

    if (dx == +1) return !grid_.has_wall(from_x, from_z, world::TileGrid::Side::East);
    if (dx == -1) return !grid_.has_wall(to_x,   to_z,   world::TileGrid::Side::East);
    if (dz == +1) return !grid_.has_wall(from_x, from_z, world::TileGrid::Side::North);
    /* dz == -1 */ return !grid_.has_wall(to_x,   to_z,   world::TileGrid::Side::North);
}

Path Pathfinder::find(const glm::ivec2& start, const glm::ivec2& goal) const {
    Path out;
    if (!grid_.in_bounds_tile(start.x, start.y) ||
        !grid_.in_bounds_tile(goal.x, goal.y)) {
        return out;
    }

    const Key s{start.x, start.y};
    const Key g{goal.x, goal.y};

    if (s == g) {
        out.waypoints.push_back({s.x, s.z});
        out.valid = true;
        return out;
    }

    auto h = [g](const Key& k) {
        return std::abs(k.x - g.x) + std::abs(k.z - g.z);
    };

    std::unordered_map<Key, int, KeyHash> g_score;
    std::unordered_map<Key, Key, KeyHash> came_from;
    std::unordered_map<Key, bool, KeyHash> closed;

    g_score[s] = 0;
    std::priority_queue<Node, std::vector<Node>, NodeCmp> open;
    open.push({s, h(s)});

    bool found = false;
    while (!open.empty()) {
        Node cur = open.top();
        open.pop();
        if (closed[cur.key]) continue;
        closed[cur.key] = true;

        if (cur.key == g) { found = true; break; }

        for (const auto& n : kNeighbors) {
            Key nb{cur.key.x + n.dx, cur.key.z + n.dz};
            if (!grid_.in_bounds_tile(nb.x, nb.z)) continue;
            if (closed[nb]) continue;

            int fx = cur.key.x, fz = cur.key.z;
            int tx = nb.x, tz = nb.z;
            bool blocked;
            if (n.dx == +1)      blocked = grid_.has_wall(fx, fz, world::TileGrid::Side::East);
            else if (n.dx == -1) blocked = grid_.has_wall(tx, tz, world::TileGrid::Side::East);
            else if (n.dz == +1) blocked = grid_.has_wall(fx, fz, world::TileGrid::Side::North);
            else                 blocked = grid_.has_wall(tx, tz, world::TileGrid::Side::North);
            if (blocked) continue;

            int tentative = g_score[cur.key] + 1;
            auto it = g_score.find(nb);
            if (it == g_score.end() || tentative < it->second) {
                g_score[nb] = tentative;
                came_from[nb] = cur.key;
                open.push({nb, tentative + h(nb)});
            }
        }
    }

    if (!found) return out;

    // Reconstruct from goal back to start.
    std::vector<glm::ivec2> rev;
    rev.push_back({g.x, g.z});
    Key c = g;
    while (c != s) {
        c = came_from.at(c);
        rev.push_back({c.x, c.z});
    }
    std::reverse(rev.begin(), rev.end());
    out.waypoints = std::move(rev);
    out.valid = true;
    return out;
}

} // namespace pathfinding
} // namespace sims

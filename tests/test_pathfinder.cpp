#include "sim/pathfinding/Pathfinder.hpp"

#include <catch2/catch_test_macros.hpp>

#include "sim/world/TileGrid.hpp"

using sims::pathfinding::Pathfinder;
using sims::world::TileGrid;

TEST_CASE("Empty grid: straight-line path", "[pathfinder]") {
    TileGrid grid(8, 8);
    Pathfinder pf(grid);
    auto path = pf.find({0, 0}, {5, 0});
    REQUIRE(path.valid);
    REQUIRE(path.size() == 6);
    REQUIRE(path.waypoints.front() == glm::ivec2{0, 0});
    REQUIRE(path.waypoints.back() == glm::ivec2{5, 0});
}

TEST_CASE("Start equals goal", "[pathfinder]") {
    TileGrid grid(4, 4);
    Pathfinder pf(grid);
    auto path = pf.find({2, 2}, {2, 2});
    REQUIRE(path.valid);
    REQUIRE(path.size() == 1);
    REQUIRE(path.empty()); // single-tile path is "empty" (no steps)
}

TEST_CASE("Out-of-bounds endpoints are invalid", "[pathfinder]") {
    TileGrid grid(4, 4);
    Pathfinder pf(grid);
    REQUIRE(!pf.find({-1, 0}, {2, 2}).valid);
    REQUIRE(!pf.find({0, 0}, {4, 0}).valid);
    REQUIRE(!pf.find({0, 0}, {0, 9}).valid);
}

TEST_CASE("Wall blocks direct route, detour found", "[pathfinder]") {
    TileGrid grid(5, 5);
    // Wall off the East edges of tiles (0,2),(1,2),(2,2),(3,2) — a vertical
    // wall line splitting the lot at x=1..4 line on row z=2. This blocks
    // moving from (0,2)->(1,2).
    for (int x = 0; x < 4; ++x) grid.add_wall(x, 2, TileGrid::Side::East);

    Pathfinder pf(grid);
    auto path = pf.find({0, 2}, {4, 2});
    REQUIRE(path.valid);
    REQUIRE(path.waypoints.front() == glm::ivec2{0, 2});
    REQUIRE(path.waypoints.back() == glm::ivec2{4, 2});
    // Path length must exceed the Manhattan distance (5) since a detour is
    // required around the wall.
    REQUIRE(path.size() > 5);
}

TEST_CASE("Fully walled-off goal is unreachable", "[pathfinder]") {
    TileGrid grid(3, 3);
    // Box in tile (1,1) with walls on all four of its edges.
    grid.add_wall(1, 1, TileGrid::Side::East);   // (1,1)-(2,1)
    grid.add_wall(0, 1, TileGrid::Side::East);   // (0,1)-(1,1)
    grid.add_wall(1, 1, TileGrid::Side::North);  // (1,1)-(1,2)
    grid.add_wall(1, 0, TileGrid::Side::North);  // (1,0)-(1,1)

    Pathfinder pf(grid);
    auto path = pf.find({0, 0}, {1, 1});
    REQUIRE(!path.valid);
    REQUIRE(path.waypoints.empty());
}

TEST_CASE("passable() respects walls and bounds", "[pathfinder]") {
    TileGrid grid(4, 4);
    grid.add_wall(1, 1, TileGrid::Side::East);
    Pathfinder pf(grid);

    REQUIRE(pf.passable(0, 0, 1, 0));
    REQUIRE(pf.passable(1, 1, 1, 2));
    REQUIRE(!pf.passable(1, 1, 2, 1)); // wall on East of (1,1)
    REQUIRE(!pf.passable(2, 1, 1, 1)); // wall on East of (1,1) (other dir)
    REQUIRE(!pf.passable(3, 0, 4, 0)); // out of bounds
    REQUIRE(!pf.passable(0, 0, 1, 1)); // diagonal
}

TEST_CASE("Path is optimal (Manhattan length)", "[pathfinder]") {
    TileGrid grid(10, 10);
    Pathfinder pf(grid);
    auto path = pf.find({0, 0}, {7, 3});
    REQUIRE(path.valid);
    // Optimal 4-connected path length = Manhattan distance + 1 (start tile).
    REQUIRE(path.size() == static_cast<std::size_t>(7 + 3 + 1));
}

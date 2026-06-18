#include "content/InteractionLibrary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ecs/registry.hpp"
#include "ecs/components.hpp"
#include "sim/SaveLoad.hpp"
#include "sim/motives/Motives.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/time/SimClock.hpp"
#include "sim/world/TileGrid.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>

using namespace sims;
using sims::content::InteractionLibrary;

namespace {

bool write_file(const std::string& path, const std::string& body) {
    std::ofstream out(path);
    if (!out) return false;
    out << body;
    return static_cast<bool>(out);
}

} // namespace

TEST_CASE("SaveLoad round-trip preserves world state", "[save_load]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/sl_i.json";
    std::string opath = dir + "/sl_o.json";
    std::string spath = dir + "/save_roundtrip.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [
        { "id": "eat", "label": "Eat", "duration_min": 30, "target": "Object" }
    ]})"));
    REQUIRE(write_file(opath, R"({ "objects": [
        { "id": "fridge", "label": "Fridge", "color": [0.8,0.8,0.9],
          "footprint": [1,1], "interactions": ["eat"] }
    ]})"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));

    Registry reg;
    world::TileGrid grid(10, 10);
    grid.add_wall(2, 3, world::TileGrid::Side::East);
    grid.add_wall(5, 7, world::TileGrid::Side::North);

    time::SimClock clock;
    clock.set_absolute_minutes(1234.5);
    clock.set_minutes_per_second(5.0);

    Entity sim = reg.create();
    glm::vec3 pos = grid.coord().tile_to_world(3, 4);
    reg.emplace<Transform>(sim, Transform{pos, 42.0f, {1,1,1}});
    reg.emplace<Sim>(sim, Sim{"Kim", 90.0f, 0.25f, true});
    reg.emplace<Movement>(sim, Movement{pos, pos, 1.5f, false, {}});
    Motives mo;
    mo.set(Motives::Hunger, 30.0f);
    mo.set(Motives::Energy, 60.0f);
    mo.set(Motives::Bladder, 10.0f);
    mo.set(Motives::Fun, 80.0f);
    mo.set(Motives::Social, 50.0f);
    reg.emplace<Motives>(sim, mo);
    reg.emplace<ActionQueue>(sim, ActionQueue{});

    Entity fridge = reg.create();
    reg.emplace<Transform>(fridge, Transform{grid.coord().tile_to_world(7,1), 0, {1,1,1}});
    reg.emplace<WorldObject>(fridge, WorldObject{"fridge", 7, 1, 1, 1, {1,1,1}});

    REQUIRE(SaveLoad::save_to_file(spath, reg, grid, clock));

    // Mutate everything, then load back.
    reg.clear();
    grid.clear_walls();
    grid.resize(4, 4);
    clock.set_absolute_minutes(0.0);
    clock.set_minutes_per_second(1.0);

    REQUIRE(SaveLoad::load_from_file(spath, reg, grid, clock, lib));

    // Grid dimensions + walls.
    REQUIRE(grid.width() == 10);
    REQUIRE(grid.depth() == 10);
    REQUIRE(grid.wall_count() == 2);
    REQUIRE(grid.has_wall(2, 3, world::TileGrid::Side::East));
    REQUIRE(grid.has_wall(5, 7, world::TileGrid::Side::North));

    // Clock.
    REQUIRE_THAT(clock.minutes(), Catch::Matchers::WithinAbs(1234.5, 1e-3));
    REQUIRE_THAT(clock.minutes_per_second(), Catch::Matchers::WithinAbs(5.0, 1e-3));

    // Exactly one Sim + one object.
    int sim_count = 0, obj_count = 0;
    auto sv = reg.view<Sim>();
    for (auto e : sv) {
        ++sim_count;
        const auto& s = sv.get<Sim>(e);
        const auto& t = reg.get<Transform>(e);
        const auto& m = reg.get<Movement>(e);
        const auto& mm = reg.get<Motives>(e);
        REQUIRE(s.name == "Kim");
        REQUIRE(s.facing_deg == 90.0f);
        REQUIRE(s.walk_phase == 0.25f);
        REQUIRE(s.moving);
        REQUIRE(t.yaw_deg == 42.0f);
        REQUIRE_THAT(t.position.x, Catch::Matchers::WithinAbs(pos.x, 1e-4f));
        REQUIRE_THAT(t.position.z, Catch::Matchers::WithinAbs(pos.z, 1e-4f));
        REQUIRE_THAT(m.speed, Catch::Matchers::WithinAbs(1.5f, 1e-4f));
        REQUIRE_THAT(mm.get(Motives::Hunger), Catch::Matchers::WithinAbs(30.0f, 1e-4f));
        REQUIRE_THAT(mm.get(Motives::Bladder), Catch::Matchers::WithinAbs(10.0f, 1e-4f));
    }
    auto ov = reg.view<WorldObject>();
    for (auto e : ov) {
        ++obj_count;
        const auto& wo = ov.get<WorldObject>(e);
        REQUIRE(wo.def_id == "fridge");
        REQUIRE(wo.tile_x == 7);
        REQUIRE(wo.tile_z == 1);
    }
    REQUIRE(sim_count == 1);
    REQUIRE(obj_count == 1);
}

TEST_CASE("SaveLoad load skips unknown object def_ids", "[save_load]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/sl_unknown_i.json";
    std::string opath = dir + "/sl_unknown_o.json";
    std::string spath = dir + "/save_unknown.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [] })"));
    REQUIRE(write_file(opath, R"({ "objects": [] })"));
    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));
    // Build a save file by hand with an unknown def_id.
    REQUIRE(write_file(spath, R"({
        "version": 1,
        "clock": { "sim_minutes": 0.0, "minutes_per_second": 1.0 },
        "grid": { "w": 4, "d": 4, "walls": [] },
        "sims": [],
        "objects": [
            { "def_id": "ghost", "tile_x": 1, "tile_z": 1 }
        ]
    })"));

    Registry reg;
    world::TileGrid grid(4, 4);
    time::SimClock clock;
    REQUIRE(SaveLoad::load_from_file(spath, reg, grid, clock, lib));
    REQUIRE(reg.view<WorldObject>().empty());
}

TEST_CASE("SaveLoad rejects unsupported version", "[save_load]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string spath = dir + "/save_badver.json";
    REQUIRE(write_file(spath, R"({ "version": 999, "clock": {}, "grid": {}, "sims": [], "objects": [] })"));

    Registry reg;
    world::TileGrid grid(4, 4);
    time::SimClock clock;
    InteractionLibrary lib;
    REQUIRE_FALSE(SaveLoad::load_from_file(spath, reg, grid, clock, lib));
}

TEST_CASE("SaveLoad missing file returns false", "[save_load]") {
    Registry reg;
    world::TileGrid grid(4, 4);
    time::SimClock clock;
    InteractionLibrary lib;
    REQUIRE_FALSE(SaveLoad::load_from_file("/nonexistent/save.json",
                                           reg, grid, clock, lib));
}

TEST_CASE("SaveLoad empty world round-trips", "[save_load]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string spath = dir + "/save_empty.json";
    Registry reg;
    world::TileGrid grid(8, 8);
    time::SimClock clock;
    clock.set_absolute_minutes(42.0);

    REQUIRE(SaveLoad::save_to_file(spath, reg, grid, clock));

    (void)reg.create(); // dirty the registry
    grid.add_wall(0, 0, world::TileGrid::Side::East);
    clock.set_absolute_minutes(0.0);

    REQUIRE(SaveLoad::load_from_file(spath, reg, grid, clock, {}));
    REQUIRE(reg.view<Sim>().empty());
    REQUIRE(reg.view<WorldObject>().empty());
    REQUIRE(grid.wall_count() == 0);
    REQUIRE_THAT(clock.minutes(), Catch::Matchers::WithinAbs(42.0, 1e-3));
}

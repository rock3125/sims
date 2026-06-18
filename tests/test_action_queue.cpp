#include "content/InteractionLibrary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ecs/registry.hpp"
#include "sim/ActionSystem.hpp"
#include "sim/MovementSystem.hpp"
#include "sim/motives/Motives.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/world/TileGrid.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>

using namespace sims;
using sims::content::InteractionDef;
using sims::content::InteractionLibrary;
using sims::content::InteractionTarget;
using sims::content::ObjectDef;

namespace {

// Build a library from inline JSON paths so tests don't depend on the
// on-disk asset directory. Writes to /tmp and loads from there.
bool write_file(const std::string& path, const std::string& body) {
    std::ofstream out(path);
    if (!out) return false;
    out << body;
    return static_cast<bool>(out);
}

} // namespace

TEST_CASE("InteractionLibrary loads valid JSON", "[interaction_lib]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/interactions.json";
    std::string opath = dir + "/objects.json";
    REQUIRE(write_file(ipath, R"({
      "interactions": [
        { "id": "eat", "label": "Eat", "category": "Hunger",
          "duration_min": 30,
          "motive_deltas": { "Hunger": 50, "Energy": -5 },
          "target": "Object" },
        { "id": "nap", "label": "Nap", "category": "Energy",
          "duration_min": 120,
          "motive_deltas": { "Energy": 80 },
          "target": "Self" }
      ]
    })"));
    REQUIRE(write_file(opath, R"({
      "objects": [
        { "id": "fridge", "label": "Fridge",
          "color": [0.8, 0.8, 0.9], "footprint": [1, 1],
          "interactions": ["eat"] }
      ]
    })"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));
    REQUIRE_FALSE(lib.empty());

    const auto* eat = lib.interaction("eat");
    REQUIRE(eat);
    REQUIRE(eat->label == "Eat");
    REQUIRE(eat->duration_min == 30.0);
    REQUIRE(eat->target == InteractionTarget::Object);
    REQUIRE_THAT(eat->motive_deltas[0], Catch::Matchers::WithinAbs(50.0f, 1e-4f));
    REQUIRE_THAT(eat->motive_deltas[1], Catch::Matchers::WithinAbs(-5.0f, 1e-4f));

    const auto* nap = lib.interaction("nap");
    REQUIRE(nap);
    REQUIRE(nap->target == InteractionTarget::Self);

    const auto* fridge = lib.object("fridge");
    REQUIRE(fridge);
    REQUIRE(fridge->label == "Fridge");
    REQUIRE(fridge->footprint_w == 1);
    REQUIRE(fridge->footprint_d == 1);
    REQUIRE(fridge->interaction_ids.size() == 1);
    REQUIRE(fridge->interaction_ids[0] == "eat");
}

TEST_CASE("InteractionLibrary missing file returns false", "[interaction_lib]") {
    InteractionLibrary lib;
    REQUIRE_FALSE(lib.load("/nonexistent/interactions.json",
                           "/nonexistent/objects.json"));
}

TEST_CASE("Unknown interaction ids on objects are dropped", "[interaction_lib]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/i2.json";
    std::string opath = dir + "/o2.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [
        { "id": "eat", "label": "Eat", "duration_min": 10, "target": "Object" }
    ]})"));
    REQUIRE(write_file(opath, R"({ "objects": [
        { "id": "x", "label": "X", "footprint": [1,1],
          "interactions": ["eat", "nonexistent"] }
    ]})"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));
    const auto* x = lib.object("x");
    REQUIRE(x);
    REQUIRE(x->interaction_ids.size() == 1);
    REQUIRE(x->interaction_ids[0] == "eat");
}

TEST_CASE("ActionQueue enqueue + Self-target performs immediately", "[action_queue]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/i3.json";
    std::string opath = dir + "/o3.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [
        { "id": "nap", "label": "Nap", "duration_min": 60,
          "motive_deltas": { "Energy": 80 }, "target": "Self" }
    ]})"));
    REQUIRE(write_file(opath, R"({ "objects": [] })"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));

    world::TileGrid grid(4, 4);
    pathfinding::Pathfinder pf(grid);
    ActionSystem sys(lib, pf, grid.coord());

    Registry reg;
    Entity sim = reg.create();
    reg.emplace<Transform>(sim, Transform{{0,0,0}, 0, {1,1,1}});
    reg.emplace<Sim>(sim, Sim{});
    reg.emplace<Movement>(sim, Movement{});
    reg.emplace<Motives>(sim, Motives{});
    reg.emplace<ActionQueue>(sim, ActionQueue{});

    REQUIRE(sys.enqueue(reg, sim, "nap"));
    REQUIRE(reg.get<ActionQueue>(sim).queue.size() == 1);
    REQUIRE(reg.get<ActionQueue>(sim).current()->phase == ActionQueue::Phase::Performing);

    // Tick halfway: still performing.
    sys.update(reg, 30.0);
    REQUIRE(reg.get<ActionQueue>(sim).active());
    REQUIRE_THAT(reg.get<Motives>(sim).get(Motives::Energy),
                 Catch::Matchers::WithinAbs(100.0f, 1e-4f)); // not applied yet

    // Complete.
    sys.update(reg, 30.0);
    REQUIRE_FALSE(reg.get<ActionQueue>(sim).active());
    // Energy was 100, +80, clamped to 100.
    REQUIRE_THAT(reg.get<Motives>(sim).get(Motives::Energy),
                 Catch::Matchers::WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("ActionQueue Object-target: path + perform applies deltas", "[action_queue]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/i4.json";
    std::string opath = dir + "/o4.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [
        { "id": "eat", "label": "Eat", "duration_min": 20,
          "motive_deltas": { "Hunger": 40 }, "target": "Object" }
    ]})"));
    REQUIRE(write_file(opath, R"({ "objects": [
        { "id": "fridge", "label": "Fridge", "color": [1,1,1], "footprint": [1,1],
          "interactions": ["eat"] }
    ]})"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));

    world::TileGrid grid(6, 6);
    pathfinding::Pathfinder pf(grid);
    ActionSystem sys(lib, pf, grid.coord());
    MovementSystem mv;

    Registry reg;
    Entity sim = reg.create();
    glm::vec3 start = grid.coord().tile_to_world(0, 0);
    reg.emplace<Transform>(sim, Transform{start, 0, {1,1,1}});
    reg.emplace<Sim>(sim, Sim{});
    reg.emplace<Movement>(sim, Movement{start, start, 2.0f, false, {}});
    reg.emplace<Motives>(sim, Motives{});
    reg.emplace<ActionQueue>(sim, ActionQueue{});

    Entity fridge = reg.create();
    glm::vec3 fp = grid.coord().tile_to_world(5, 0);
    reg.emplace<Transform>(fridge, Transform{fp, 0, {1,1,1}});
    reg.emplace<WorldObject>(fridge, WorldObject{"fridge", 5, 0, 1, 1, {1,1,1}});

    REQUIRE(sys.enqueue(reg, sim, "eat", fridge));
    REQUIRE(reg.get<ActionQueue>(sim).current()->phase == ActionQueue::Phase::PendingMove);

    // First tick: starts the action, computes path, phase -> Moving.
    sys.update(reg, 0.0);
    REQUIRE(reg.get<ActionQueue>(sim).current()->phase == ActionQueue::Phase::Moving);
    REQUIRE(reg.get<Movement>(sim).has_target);

    // Drive movement until arrival.
    for (int i = 0; i < 200; ++i) {
        mv.update(reg, 0.1f);
        sys.update(reg, 0.0);
        if (!reg.get<ActionQueue>(sim).active()) break;
        auto ph = reg.get<ActionQueue>(sim).current()->phase;
        if (ph == ActionQueue::Phase::Performing) break;
    }
    REQUIRE(reg.get<ActionQueue>(sim).active());
    REQUIRE(reg.get<ActionQueue>(sim).current()->phase == ActionQueue::Phase::Performing);

    // Deplete hunger a bit, then complete the action.
    reg.get<Motives>(sim).set(Motives::Hunger, 30.0f);
    sys.update(reg, 20.0);
    REQUIRE_FALSE(reg.get<ActionQueue>(sim).active());
    REQUIRE_THAT(reg.get<Motives>(sim).get(Motives::Hunger),
                 Catch::Matchers::WithinAbs(70.0f, 1e-4f)); // 30 + 40
}

TEST_CASE("ActionQueue cancel clears queue and stops movement", "[action_queue]") {
    world::TileGrid grid(4, 4);
    InteractionLibrary lib;
    pathfinding::Pathfinder pf(grid);
    ActionSystem sys(lib, pf, grid.coord());

    Registry reg;
    Entity sim = reg.create();
    reg.emplace<Transform>(sim, Transform{});
    reg.emplace<Sim>(sim, Sim{});
    reg.emplace<Movement>(sim, Movement{});
    reg.emplace<Motives>(sim, Motives{});
    auto& q = reg.emplace<ActionQueue>(sim, ActionQueue{});
    q.queue.push_back({});
    q.queue.push_back({});
    reg.get<Movement>(sim).has_target = true;

    ActionSystem::cancel(reg, sim);
    REQUIRE_FALSE(reg.get<ActionQueue>(sim).active());
    REQUIRE_FALSE(reg.get<Movement>(sim).has_target);
}

TEST_CASE("ActionQueue aborts when no path to object", "[action_queue]") {
    std::string dir = "/tmp/opencode/sims_tests";
    std::filesystem::create_directories(dir);
    std::string ipath = dir + "/i5.json";
    std::string opath = dir + "/o5.json";
    REQUIRE(write_file(ipath, R"({ "interactions": [
        { "id": "eat", "label": "Eat", "duration_min": 10,
          "motive_deltas": { "Hunger": 40 }, "target": "Object" }
    ]})"));
    REQUIRE(write_file(opath, R"({ "objects": [
        { "id": "fridge", "label": "Fridge", "color": [1,1,1], "footprint": [1,1],
          "interactions": ["eat"] }
    ]})"));

    InteractionLibrary lib;
    REQUIRE(lib.load(ipath, opath));

    // 3x1 grid: Sim at (0,0), fridge at (2,0). Wall (0,0)-(1,0) and
    // (1,0)-(2,0) so the only adjacent tile (1,0) is unreachable from (0,0).
    world::TileGrid grid(3, 1);
    grid.add_wall(0, 0, world::TileGrid::Side::East); // (0,0)-(1,0)
    grid.add_wall(1, 0, world::TileGrid::Side::East); // (1,0)-(2,0)

    pathfinding::Pathfinder pf(grid);
    ActionSystem sys(lib, pf, grid.coord());

    Registry reg;
    Entity sim = reg.create();
    glm::vec3 start = grid.coord().tile_to_world(0, 0);
    reg.emplace<Transform>(sim, Transform{start, 0, {1,1,1}});
    reg.emplace<Sim>(sim, Sim{});
    reg.emplace<Movement>(sim, Movement{start, start, 2.0f, false, {}});
    reg.emplace<Motives>(sim, Motives{});
    reg.emplace<ActionQueue>(sim, ActionQueue{});

    Entity fridge = reg.create();
    glm::vec3 fp = grid.coord().tile_to_world(2, 0);
    reg.emplace<Transform>(fridge, Transform{fp, 0, {1,1,1}});
    reg.emplace<WorldObject>(fridge, WorldObject{"fridge", 2, 0, 1, 1, {1,1,1}});

    REQUIRE(sys.enqueue(reg, sim, "eat", fridge));
    sys.update(reg, 0.0); // attempts start_action -> no path -> abort
    REQUIRE_FALSE(reg.get<ActionQueue>(sim).active());
    REQUIRE_FALSE(reg.get<Movement>(sim).has_target);
}

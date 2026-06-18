#include "content/InteractionLibrary.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ecs/registry.hpp"
#include "sim/ActionSystem.hpp"
#include "sim/AutonomySystem.hpp"
#include "sim/MovementSystem.hpp"
#include "sim/motives/Motives.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/world/TileGrid.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <fstream>

using namespace sims;
using sims::content::InteractionLibrary;
using sims::content::InteractionTarget;

namespace {

bool write_file(const std::string& path, const std::string& body) {
    std::ofstream out(path);
    if (!out) return false;
    out << body;
    return static_cast<bool>(out);
}

struct Fixture {
    std::string dir = "/tmp/opencode/sims_tests";
    std::string ipath = dir + "/auto_i.json";
    std::string opath = dir + "/auto_o.json";
    InteractionLibrary lib;
    world::TileGrid grid{8, 8};
    pathfinding::Pathfinder pf{grid};
    Registry reg;
    ActionSystem actions;
    AutonomySystem auto_sys;

    Fixture() : actions(lib, pf, grid.coord()),
                auto_sys(lib, pf, grid.coord(), actions) {
        std::filesystem::create_directories(dir);
        REQUIRE(write_file(ipath, R"({ "interactions": [
            { "id": "eat", "label": "Eat", "duration_min": 30,
              "motive_deltas": { "Hunger": 50, "Energy": -5, "Bladder": -10 },
              "target": "Object" },
            { "id": "sleep", "label": "Sleep", "duration_min": 240,
              "motive_deltas": { "Energy": 100, "Hunger": -20 },
              "target": "Object" },
            { "id": "chat", "label": "Chat", "duration_min": 20,
              "motive_deltas": { "Social": 30, "Fun": 5 },
              "target": "Self" }
        ]})"));
        REQUIRE(write_file(opath, R"({ "objects": [
            { "id": "fridge", "label": "Fridge", "color": [1,1,1],
              "footprint": [1,1], "interactions": ["eat"] },
            { "id": "bed",    "label": "Bed",    "color": [1,1,1],
              "footprint": [1,1], "interactions": ["sleep"] }
        ]})"));
        REQUIRE(lib.load(ipath, opath));
    }

    Entity make_sim(int tx, int tz) {
        Entity e = reg.create();
        glm::vec3 p = grid.coord().tile_to_world(tx, tz);
        reg.emplace<Transform>(e, Transform{p, 0, {1,1,1}});
        reg.emplace<Sim>(e, Sim{});
        reg.emplace<Movement>(e, Movement{p, p, 2.0f, false, {}});
        reg.emplace<Motives>(e, Motives{});
        reg.emplace<ActionQueue>(e, ActionQueue{});
        return e;
    }

    Entity make_object(const std::string& id, int tx, int tz) {
        const auto* def = lib.object(id);
        REQUIRE(def);
        Entity e = reg.create();
        reg.emplace<Transform>(e, Transform{grid.coord().tile_to_world(tx,tz),
                                            0, {1,1,1}});
        reg.emplace<WorldObject>(e, WorldObject{id, tx, tz,
            def->footprint_w, def->footprint_d, def->color});
        return e;
    }
};

} // namespace

TEST_CASE("Autonomy score: urgency weights", "[autonomy]") {
    Fixture f;
    const auto* eat = f.lib.interaction("eat");
    REQUIRE(eat);

    Motives full;       // all 100
    Motives starving;   // Hunger 0, rest 100
    starving.set(Motives::Hunger, 0.0f);

    // Full motives: positive deltas contribute nothing; negative deltas also
    // contribute nothing (urgency 0). Only the duration/travel cost remains.
    float s_full = f.auto_sys.score(full, *eat, 0);
    REQUIRE(s_full < 0.0f); // pure cost penalty

    // Starving: Hunger +50 * urgency(0)=1.0 = +50. Energy -5 * urgency(100)=0
    // = 0. Bladder -10 * urgency(100)=0 = 0. Minus duration cost 0.05*30=1.5.
    float s_starv = f.auto_sys.score(starving, *eat, 0);
    REQUIRE_THAT(s_starv, Catch::Matchers::WithinAbs(48.5f, 1e-3f));

    // Travel penalty applies.
    float s_travel = f.auto_sys.score(starving, *eat, 5);
    REQUIRE_THAT(s_travel, Catch::Matchers::WithinAbs(48.5f - 0.10f * 5.0f, 1e-3f));
}

TEST_CASE("Autonomy: no action when motives are full", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    f.make_object("fridge", 2, 0);
    f.make_object("bed", 0, 2);

    f.auto_sys.update(f.reg, 1.0);
    REQUIRE_FALSE(f.reg.get<ActionQueue>(sim).active());
}

TEST_CASE("Autonomy: enqueues best interaction for low motive", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    Entity fridge = f.make_object("fridge", 2, 0);
    f.make_object("bed", 0, 2);

    f.reg.get<Motives>(sim).set(Motives::Hunger, 0.0f);

    f.auto_sys.update(f.reg, 1.0);
    REQUIRE(f.reg.get<ActionQueue>(sim).active());
    const auto* cur = f.reg.get<ActionQueue>(sim).current();
    REQUIRE(cur->interaction_id == "eat");
    REQUIRE(cur->target_object == fridge);
}

TEST_CASE("Autonomy: picks Self interaction when no object helps", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    // Only a fridge present; Social is depleted -> only `chat` (Self) helps.
    f.make_object("fridge", 2, 0);
    f.reg.get<Motives>(sim).set(Motives::Social, 0.0f);

    f.auto_sys.update(f.reg, 1.0);
    REQUIRE(f.reg.get<ActionQueue>(sim).active());
    REQUIRE(f.reg.get<ActionQueue>(sim).current()->interaction_id == "chat");
    REQUIRE(f.reg.get<ActionQueue>(sim).current()->target_object == static_cast<Entity>(entt::null));
}

TEST_CASE("Autonomy: respects cooldown and skips busy Sims", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    f.make_object("fridge", 2, 0);
    f.reg.get<Motives>(sim).set(Motives::Hunger, 0.0f);

    // First tick enqueues.
    f.auto_sys.update(f.reg, 0.1);
    REQUIRE(f.reg.get<ActionQueue>(sim).active());
    std::size_t first_size = f.reg.get<ActionQueue>(sim).queue.size();
    REQUIRE(first_size == 1u);

    // While busy, autonomy must not append more actions even though the
    // motive is still low.
    f.auto_sys.update(f.reg, 0.1);
    REQUIRE(f.reg.get<ActionQueue>(sim).queue.size() == first_size);

    // Cancel, then immediately tick: cooldown should block re-decision.
    ActionSystem::cancel(f.reg, sim);
    REQUIRE_FALSE(f.reg.get<ActionQueue>(sim).active());
    f.auto_sys.update(f.reg, 0.1);
    REQUIRE_FALSE(f.reg.get<ActionQueue>(sim).active());

    // After cooldown elapses, it decides again.
    f.auto_sys.update(f.reg, 5.0);
    REQUIRE(f.reg.get<ActionQueue>(sim).active());
}

TEST_CASE("Autonomy: disabled flag prevents enqueuing", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    f.make_object("fridge", 2, 0);
    f.reg.get<Motives>(sim).set(Motives::Hunger, 0.0f);

    f.auto_sys.enabled = false;
    f.auto_sys.update(f.reg, 5.0);
    REQUIRE_FALSE(f.reg.get<ActionQueue>(sim).active());
}

TEST_CASE("Autonomy: threshold avoids acting on marginal gains", "[autonomy]") {
    Fixture f;
    Entity sim = f.make_sim(0, 0);
    f.make_object("fridge", 2, 0);
    // Hunger slightly below full: urgency small, score below min_score.
    f.reg.get<Motives>(sim).set(Motives::Hunger, 95.0f);

    f.auto_sys.update(f.reg, 5.0);
    REQUIRE_FALSE(f.reg.get<ActionQueue>(sim).active());
}

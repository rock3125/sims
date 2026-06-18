#include "sim/MotiveSystem.hpp"
#include "sim/motives/Motives.hpp"
#include "ecs/registry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using sims::Entity;
using sims::Motives;
using sims::MotiveSystem;
using sims::Registry;

TEST_CASE("Motives default to 100", "[motives]") {
    Motives m;
    for (std::size_t i = 0; i < Motives::kCount; ++i) {
        REQUIRE(m.value[i] == 100.0f);
    }
    REQUIRE_THAT(m.average(), Catch::Matchers::WithinAbs(100.0f, 1e-4f));
}

TEST_CASE("set() clamps to [0, 100]", "[motives]") {
    Motives m;
    m.set(Motives::Hunger, 150.0f);
    REQUIRE(m.get(Motives::Hunger) == 100.0f);
    m.set(Motives::Energy, -5.0f);
    REQUIRE(m.get(Motives::Energy) == 0.0f);
    m.set(Motives::Fun, 42.0f);
    REQUIRE(m.get(Motives::Fun) == 42.0f);
}

TEST_CASE("adjust() adds delta with clamping", "[motives]") {
    Motives m;
    m.adjust(Motives::Bladder, -30.0f);
    REQUIRE(m.get(Motives::Bladder) == 70.0f);
    m.adjust(Motives::Bladder, -100.0f); // would go below 0
    REQUIRE(m.get(Motives::Bladder) == 0.0f);
    m.adjust(Motives::Social, +5.0f);    // already 100 -> stays 100
    REQUIRE(m.get(Motives::Social) == 100.0f);
}

TEST_CASE("MotiveSystem decays all motives by sim-minutes", "[motives_system]") {
    Registry reg;
    Entity e = reg.create();
    reg.emplace<Motives>(e, Motives{});

    MotiveSystem sys;
    sys.update(reg, 60.0); // 1 sim-hour

    const auto& m = reg.get<Motives>(e);
    // Hunger decays at -0.10/min -> after 60 min: 100 - 6 = 94
    REQUIRE_THAT(m.get(Motives::Hunger),  Catch::Matchers::WithinAbs(94.0f, 1e-4f));
    // Energy decays at -0.20/min -> 100 - 12 = 88
    REQUIRE_THAT(m.get(Motives::Energy),  Catch::Matchers::WithinAbs(88.0f, 1e-4f));
    // Bladder at -0.15/min -> 100 - 9 = 91
    REQUIRE_THAT(m.get(Motives::Bladder), Catch::Matchers::WithinAbs(91.0f, 1e-4f));
    // Fun at -0.10/min -> 94
    REQUIRE_THAT(m.get(Motives::Fun),     Catch::Matchers::WithinAbs(94.0f, 1e-4f));
    // Social at -0.08/min -> 100 - 4.8 = 95.2
    REQUIRE_THAT(m.get(Motives::Social),  Catch::Matchers::WithinAbs(95.2f, 1e-4f));
}

TEST_CASE("MotiveSystem clamps at 0 (no negative motives)", "[motives_system]") {
    Registry reg;
    Entity e = reg.create();
    auto& m = reg.emplace<Motives>(e, Motives{});
    m.set(Motives::Energy, 5.0f);

    MotiveSystem sys;
    sys.update(reg, 100.0); // -0.20 * 100 = -20 -> would go to -15, clamp to 0
    REQUIRE(reg.get<Motives>(e).get(Motives::Energy) == 0.0f);
}

TEST_CASE("MotiveSystem only affects entities with Motives", "[motives_system]") {
    Registry reg;
    Entity with = reg.create();
    Entity without = reg.create();
    reg.emplace<Motives>(with, Motives{});
    reg.emplace<int>(without, 42); // unrelated component

    MotiveSystem sys;
    sys.update(reg, 60.0);
    REQUIRE(reg.all_of<Motives>(with));
    REQUIRE_FALSE(reg.all_of<Motives>(without));
    REQUIRE_THAT(reg.get<Motives>(with).get(Motives::Hunger),
                 Catch::Matchers::WithinAbs(94.0f, 1e-4f));
}

TEST_CASE("Decay rates sum to a sensible daily drain", "[motives]") {
    // Sanity check: a fully-rested Sim should last more than a few sim-hours
    // before any motive hits zero (no motive depletes in under ~8 sim-hours).
    for (std::size_t i = 0; i < Motives::kCount; ++i) {
        float rate = -Motives::kDecayPerMin[i]; // positive per-min drain
        float minutes_to_deplete = 100.0f / rate;
        INFO("Motive " << Motives::kNames[i] << " depletes in "
             << minutes_to_deplete << " sim-min ("
             << minutes_to_deplete / 60.0 << " sim-hours)");
        REQUIRE(minutes_to_deplete >= 480.0f); // at least 8 sim-hours
    }
}

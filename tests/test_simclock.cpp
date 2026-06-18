#include "sim/time/SimClock.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using sims::time::SimClock;

TEST_CASE("SimClock starts at Day 0 00:00", "[simclock]") {
    SimClock c;
    REQUIRE(c.day() == 0);
    REQUIRE(c.hour_int() == 0);
    REQUIRE(c.minute_int() == 0);
    REQUIRE_THAT(c.minutes(), Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Default rate is 1 game-min per real-sec", "[simclock]") {
    SimClock c;
    REQUIRE_THAT(c.minutes_per_second(), Catch::Matchers::WithinAbs(1.0, 1e-9));
}

TEST_CASE("Tick advances sim-minutes by dt * rate", "[simclock]") {
    SimClock c;
    c.set_minutes_per_second(2.0); // 2 sim-min per real-sec
    c.tick(30.0);                  // 30 real-sec -> 60 sim-min
    REQUIRE_THAT(c.minutes(), Catch::Matchers::WithinAbs(60.0, 1e-9));
    REQUIRE(c.hour_int() == 1);
    REQUIRE(c.minute_int() == 0);
}

TEST_CASE("Day rolls over at 1440 minutes", "[simclock]") {
    SimClock c;
    c.set_minutes_per_second(1440.0); // a full day in 1 real-sec
    c.tick(1.0);
    REQUIRE(c.day() == 1);
    REQUIRE(c.hour_int() == 0);
    REQUIRE(c.minute_int() == 0);
    REQUIRE_THAT(c.minutes_of_day(), Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("hour() returns fractional hour-of-day", "[simclock]") {
    SimClock c;
    c.set_absolute_minutes(90.0); // 01:30
    REQUIRE(c.day() == 0);
    REQUIRE(c.hour_int() == 1);
    REQUIRE(c.minute_int() == 30);
    REQUIRE_THAT(c.hour(), Catch::Matchers::WithinAbs(1.5f, 1e-4f));
}

TEST_CASE("Accumulate across many ticks", "[simclock]") {
    SimClock c;
    c.set_minutes_per_second(0.5);
    for (int i = 0; i < 100; ++i) c.tick(1.0); // 50 sim-min total
    REQUIRE_THAT(c.minutes(), Catch::Matchers::WithinAbs(50.0, 1e-9));
    REQUIRE(c.hour_int() == 0);
    REQUIRE(c.minute_int() == 50);
}

TEST_CASE("Time scale change affects future ticks only", "[simclock]") {
    SimClock c;
    c.set_minutes_per_second(1.0);
    c.tick(10.0); // +10 min
    c.set_minutes_per_second(5.0);
    c.tick(2.0);  // +10 min
    REQUIRE_THAT(c.minutes(), Catch::Matchers::WithinAbs(20.0, 1e-9));
}

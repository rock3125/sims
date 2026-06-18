#pragma once

#include <cstddef>

namespace sims {
namespace time {

// Sim-world clock. Time advances at a configurable rate (game minutes per
// real second). A "day" is 1440 sim-minutes; default rate is 1.0
// game-min/real-sec, so one full sim-day takes 24 real minutes.
//
// All accessors are in sim-minutes unless noted. The clock is a value type
// owned by the Application and ticked once per fixed sim step.
class SimClock {
public:
    SimClock() = default;

    // Advance the clock by `real_seconds` of real time. Sim-minutes added =
    // real_seconds * minutes_per_second.
    void tick(double real_seconds) {
        sim_minutes_ += real_seconds * minutes_per_second_;
    }

    // Total elapsed sim-minutes since the clock started.
    double minutes() const { return sim_minutes_; }

    // Sim-day index since start (0-based).
    int day() const { return static_cast<int>(sim_minutes_ / 1440.0); }

    // Minutes since midnight of the current sim-day [0, 1440).
    double minutes_of_day() const {
        double m = sim_minutes_ - 1440.0 * static_cast<double>(day());
        return m < 0.0 ? 0.0 : m;
    }

    // Hour-of-day [0, 24) as a float (e.g. 14.5 = 14:30).
    float hour() const { return static_cast<float>(minutes_of_day() / 60.0); }

    // Integer hour [0, 23].
    int hour_int() const { return (static_cast<int>(minutes_of_day()) / 60) % 24; }

    // Integer minute within the current hour [0, 59].
    int minute_int() const { return static_cast<int>(minutes_of_day()) % 60; }

    // Format as "Day N  HH:MM".
    void format(char* buf, std::size_t buf_size) const;

    double minutes_per_second() const { return minutes_per_second_; }
    void set_minutes_per_second(double mps) { minutes_per_second_ = mps; }

    // Set the clock to a specific minute-of-day on day 0 (used by tests).
    void set_absolute_minutes(double m) { sim_minutes_ = m; }

private:
    double sim_minutes_ = 0.0;       // total elapsed sim-minutes
    double minutes_per_second_ = 1.0; // game-min per real-sec
};

} // namespace time
} // namespace sims

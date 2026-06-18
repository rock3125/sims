#pragma once

#include <array>
#include <cstdint>

namespace sims {

// Motives (a.k.a. "needs"). Each is a 0..100 value where 100 = fully
// satisfied and 0 = critical/depleted. Motives decay over sim-time at a
// per-sim-minute rate defined in `kDecayPerMin`. They are clamped to [0, 100].
//
// Phase 5: decay only; replenishment comes in Phase 6 (interactions) and
// Phase 7 (autonomy). The five canonical Sims motives are tracked.
struct Motives {
    enum Kind : std::uint8_t {
        Hunger = 0,
        Energy,
        Bladder,
        Fun,
        Social,
        kCount
    };

    std::array<float, kCount> value{100.0f, 100.0f, 100.0f, 100.0f, 100.0f};

    float get(Kind k) const { return value[static_cast<std::size_t>(k)]; }
    void set(Kind k, float v) {
        value[static_cast<std::size_t>(k)] = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    }
    void adjust(Kind k, float delta) { set(k, get(k) + delta); }

    // Decay rates per sim-minute (negative = depletes over time).
    static constexpr std::array<float, kCount> kDecayPerMin = {
        -0.10f,  // Hunger  : ~1000 sim-min (~16.6 sim-hours) to deplete
        -0.20f,  // Energy  : 500 sim-min
        -0.15f,  // Bladder : 666 sim-min
        -0.10f,  // Fun     : 1000 sim-min
        -0.08f,  // Social  : 1250 sim-min
    };

    // Display names for UI/debug.
    static constexpr const char* kNames[kCount] = {
        "Hunger", "Energy", "Bladder", "Fun", "Social"
    };

    // Mood: simple aggregate (average). Lower = worse mood. Phase 5 uses this
    // for the debug panel; Phase 7 utility AI will refine it.
    float average() const;
};

} // namespace sims

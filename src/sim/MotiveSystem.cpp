#include "sim/MotiveSystem.hpp"

#include "sim/motives/Motives.hpp"

namespace sims {

void MotiveSystem::update(Registry& reg, double sim_minutes) {
    auto view = reg.view<Motives>();
    for (auto e : view) {
        auto& m = view.get<Motives>(e);
        for (std::size_t i = 0; i < Motives::kCount; ++i) {
            const auto k = static_cast<Motives::Kind>(i);
            m.set(k, m.get(k) + Motives::kDecayPerMin[i] * static_cast<float>(sim_minutes));
        }
    }
}

} // namespace sims

#include "sim/motives/Motives.hpp"

namespace sims {

float Motives::average() const {
    float sum = 0.0f;
    for (float v : value) sum += v;
    return sum / static_cast<float>(kCount);
}

} // namespace sims

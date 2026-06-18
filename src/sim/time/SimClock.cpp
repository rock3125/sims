#include "sim/time/SimClock.hpp"

#include <cstdio>

namespace sims {
namespace time {

void SimClock::format(char* buf, std::size_t buf_size) const {
    std::snprintf(buf, buf_size, "Day %d  %02d:%02d", day(), hour_int(), minute_int());
}

} // namespace time
} // namespace sims

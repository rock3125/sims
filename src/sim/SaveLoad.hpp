#pragma once

#include "content/InteractionLibrary.hpp"
#include "ecs/registry.hpp"
#include "sim/time/SimClock.hpp"
#include "sim/world/TileGrid.hpp"

#include <string>

namespace sims {

// Phase 8: JSON save/load of the full world state.
//
// Schema (version 1):
// {
//   "version": 1,
//   "clock": { "sim_minutes": <double>, "minutes_per_second": <double> },
//   "grid":  { "w": <int>, "d": <int>,
//              "walls": [ { "x": <int>, "z": <int>, "side": "E"|"N" }, ... ] },
//   "sims":  [ { "name": <str>, "pos": [x,y,z], "yaw": <float>,
//                "facing": <float>, "walk_phase": <float>, "moving": <bool>,
//                "speed": <float>, "motives": [h,e,b,f,soc] }, ... ],
//   "objects":[ { "def_id": <str>, "tile_x": <int>, "tile_z": <int> }, ... ]
// }
//
// `load` clears the registry and grid walls first, then rebuilds. Object
// def_ids that no longer resolve in the library are skipped with a warning.
// Returns false on I/O or parse error (partial state may be left behind on
// a failed load — callers should treat the world as corrupt).
namespace SaveLoad {

bool save_to_file(const std::string& path,
                  const Registry& reg,
                  const world::TileGrid& grid,
                  const time::SimClock& clock);

// Clears `reg` (all entities) and `grid` walls before rebuilding. The grid
// is resized to the saved dimensions. The clock is overwritten. The library
// is used to resolve object def_ids and look up footprints/colors. On a
// missing/unknown def_id the object is skipped (stderr warning).
bool load_from_file(const std::string& path,
                    Registry& reg,
                    world::TileGrid& grid,
                    time::SimClock& clock,
                    const content::InteractionLibrary& lib);

} // namespace SaveLoad

} // namespace sims

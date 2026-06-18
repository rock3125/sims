#include "sim/SaveLoad.hpp"

#include "ecs/components.hpp"
#include "sim/motives/Motives.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace sims {
namespace SaveLoad {

namespace {

using json = nlohmann::json;

constexpr int kVersion = 1;

std::optional<json> read_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[save] failed to open %s\n", path.c_str());
        return std::nullopt;
    }
    try {
        json j;
        in >> j;
        return j;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[save] JSON parse error in %s: %s\n",
                     path.c_str(), e.what());
        return std::nullopt;
    }
}

const char* side_str(world::TileGrid::Side s) {
    return s == world::TileGrid::Side::East ? "E" : "N";
}

bool parse_side(const std::string& s, world::TileGrid::Side& out) {
    if (s == "E") { out = world::TileGrid::Side::East;  return true; }
    if (s == "N") { out = world::TileGrid::Side::North; return true; }
    return false;
}

} // namespace

bool save_to_file(const std::string& path,
                  const Registry& reg,
                  const world::TileGrid& grid,
                  const time::SimClock& clock) {
    json j;
    j["version"] = kVersion;
    j["clock"] = {
        {"sim_minutes", clock.minutes()},
        {"minutes_per_second", clock.minutes_per_second()},
    };
    j["grid"] = {
        {"w", grid.width()},
        {"d", grid.depth()},
        {"walls", json::array()},
    };
    for (const auto& e : grid.walls()) {
        j["grid"]["walls"].push_back({
            {"x", e.tile_x},
            {"z", e.tile_z},
            {"side", side_str(e.side)},
        });
    }

    json sims = json::array();
    auto sim_view = reg.view<Sim, Transform, Movement, Motives>();
    for (auto e : sim_view) {
        const auto& s = sim_view.get<Sim>(e);
        const auto& t = sim_view.get<Transform>(e);
        const auto& m = sim_view.get<Movement>(e);
        const auto& mo = sim_view.get<Motives>(e);
        json ms = json::array();
        for (std::size_t i = 0; i < Motives::kCount; ++i) ms.push_back(mo.value[i]);
        sims.push_back({
            {"name", s.name},
            {"pos", {t.position.x, t.position.y, t.position.z}},
            {"yaw", t.yaw_deg},
            {"facing", s.facing_deg},
            {"walk_phase", s.walk_phase},
            {"moving", s.moving},
            {"speed", m.speed},
            {"motives", ms},
        });
    }
    j["sims"] = std::move(sims);

    json objects = json::array();
    auto obj_view = reg.view<WorldObject>();
    for (auto e : obj_view) {
        const auto& wo = obj_view.get<WorldObject>(e);
        objects.push_back({
            {"def_id", wo.def_id},
            {"tile_x", wo.tile_x},
            {"tile_z", wo.tile_z},
        });
    }
    j["objects"] = std::move(objects);

    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "[save] failed to write %s\n", path.c_str());
        return false;
    }
    out << j.dump(2) << '\n';
    return static_cast<bool>(out);
}

bool load_from_file(const std::string& path,
                    Registry& reg,
                    world::TileGrid& grid,
                    time::SimClock& clock,
                    const content::InteractionLibrary& lib) {
    auto jopt = read_json_file(path);
    if (!jopt) return false;
    const auto& j = *jopt;

    if (j.value("version", 0) != kVersion) {
        std::fprintf(stderr, "[save] unsupported version %d (expected %d)\n",
                     j.value("version", 0), kVersion);
        return false;
    }

    reg.clear();
    grid.clear_walls();

    if (j.contains("clock")) {
        const auto& c = j["clock"];
        clock.set_absolute_minutes(c.value("sim_minutes", 0.0));
        clock.set_minutes_per_second(c.value("minutes_per_second", 1.0));
    }

    if (j.contains("grid")) {
        const auto& g = j["grid"];
        int w = g.value("w", grid.width());
        int d = g.value("d", grid.depth());
        grid.resize(w, d);
        if (g.contains("walls")) {
            for (const auto& wall : g["walls"]) {
                world::TileGrid::Side s{};
                if (!parse_side(wall.value("side", ""), s)) continue;
                int x = wall.value("x", 0);
                int z = wall.value("z", 0);
                if (grid.in_bounds_edge({x, z, s})) grid.add_wall(x, z, s);
            }
        }
    }

    if (j.contains("sims")) {
        for (const auto& s : j["sims"]) {
            Entity e = reg.create();
            glm::vec3 pos{0.0f};
            if (s.contains("pos") && s["pos"].size() >= 3) {
                pos = {s["pos"][0].get<float>(),
                       s["pos"][1].get<float>(),
                       s["pos"][2].get<float>()};
            }
            reg.emplace<Transform>(e, Transform{pos,
                s.value("yaw", 0.0f), {1.0f, 1.0f, 1.0f}});
            reg.emplace<Sim>(e, Sim{
                s.value("name", std::string{"Sim"}),
                s.value("facing", 0.0f),
                s.value("walk_phase", 0.0f),
                s.value("moving", false),
            });
            reg.emplace<Movement>(e, Movement{pos, pos,
                s.value("speed", 2.0f), false, {}});
            Motives mo;
            if (s.contains("motives") && s["motives"].size() >= Motives::kCount) {
                for (std::size_t i = 0; i < Motives::kCount; ++i) {
                    mo.value[i] = s["motives"][i].get<float>();
                }
            }
            reg.emplace<Motives>(e, mo);
            reg.emplace<ActionQueue>(e, ActionQueue{});
        }
    }

    if (j.contains("objects")) {
        for (const auto& o : j["objects"]) {
            std::string def_id = o.value("def_id", "");
            const auto* def = lib.object(def_id);
            if (!def) {
                std::fprintf(stderr,
                    "[save] skipping object with unknown def_id '%s'\n",
                    def_id.c_str());
                continue;
            }
            int tx = o.value("tile_x", 0);
            int tz = o.value("tile_z", 0);
            Entity e = reg.create();
            glm::vec3 pos = grid.coord().tile_to_world(tx, tz);
            pos.x += (def->footprint_w - 1) * 0.5f;
            pos.z += (def->footprint_d - 1) * 0.5f;
            reg.emplace<Transform>(e, Transform{pos, 0.0f,
                {static_cast<float>(def->footprint_w), 1.0f,
                 static_cast<float>(def->footprint_d)}});
            reg.emplace<WorldObject>(e, WorldObject{def_id, tx, tz,
                def->footprint_w, def->footprint_d, def->color});
        }
    }

    return true;
}

} // namespace SaveLoad
} // namespace sims

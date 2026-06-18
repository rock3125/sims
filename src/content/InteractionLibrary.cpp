#include "content/InteractionLibrary.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>

namespace sims {
namespace content {

namespace {

using json = nlohmann::json;

std::optional<json> read_json(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[content] failed to open %s\n", path.c_str());
        return std::nullopt;
    }
    try {
        json j;
        in >> j;
        return j;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[content] JSON parse error in %s: %s\n",
                     path.c_str(), e.what());
        return std::nullopt;
    }
}

} // namespace

bool InteractionLibrary::load(const std::string& interactions_path,
                              const std::string& objects_path) {
    auto ij = read_json(interactions_path);
    auto oj = read_json(objects_path);
    if (!ij || !oj) return false;

    interactions_.clear();
    objects_.clear();

    for (const auto& it : ij->at("interactions")) {
        InteractionDef def;
        def.id = it.at("id").get<std::string>();
        def.label = it.at("label").get<std::string>();
        def.category = it.value("category", "");
        def.duration_min = it.value("duration_min", 0.0);
        std::string tgt = it.value("target", "Object");
        def.target = (tgt == "Self") ? InteractionTarget::Self
                                     : InteractionTarget::Object;
        if (it.contains("motive_deltas")) {
            for (auto it2 = it["motive_deltas"].begin();
                 it2 != it["motive_deltas"].end(); ++it2) {
                const std::string& k = it2.key();
                float v = it2.value().get<float>();
                if      (k == "Hunger")  def.motive_deltas[0] = v;
                else if (k == "Energy")  def.motive_deltas[1] = v;
                else if (k == "Bladder") def.motive_deltas[2] = v;
                else if (k == "Fun")     def.motive_deltas[3] = v;
                else if (k == "Social")  def.motive_deltas[4] = v;
            }
        }
        interactions_[def.id] = def;
    }

    for (const auto& obj : oj->at("objects")) {
        ObjectDef def;
        def.id = obj.at("id").get<std::string>();
        def.label = obj.at("label").get<std::string>();
        auto c = obj.value("color", std::vector<float>{1.0f, 1.0f, 1.0f});
        if (c.size() >= 3) def.color = glm::vec3(c[0], c[1], c[2]);
        auto fp = obj.value("footprint", std::vector<int>{1, 1});
        if (fp.size() >= 2) {
            def.footprint_w = fp[0] < 1 ? 1 : fp[0];
            def.footprint_d = fp[1] < 1 ? 1 : fp[1];
        }
        if (obj.contains("interactions")) {
            for (const auto& iid : obj["interactions"]) {
                def.interaction_ids.push_back(iid.get<std::string>());
            }
        }
        objects_[def.id] = def;
    }

    // Drop interaction ids on objects that don't resolve.
    for (auto& [_, obj] : objects_) {
        std::vector<std::string> resolved;
        resolved.reserve(obj.interaction_ids.size());
        for (const auto& iid : obj.interaction_ids) {
            if (interactions_.contains(iid)) resolved.push_back(iid);
            else std::fprintf(stderr,
                "[content] object %s references unknown interaction %s\n",
                obj.id.c_str(), iid.c_str());
        }
        obj.interaction_ids = std::move(resolved);
    }

    return true;
}

const InteractionDef* InteractionLibrary::interaction(const std::string& id) const {
    auto it = interactions_.find(id);
    return it == interactions_.end() ? nullptr : &it->second;
}

const ObjectDef* InteractionLibrary::object(const std::string& id) const {
    auto it = objects_.find(id);
    return it == objects_.end() ? nullptr : &it->second;
}

} // namespace content
} // namespace sims

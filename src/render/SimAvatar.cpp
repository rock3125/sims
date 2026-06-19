#include "render/SimAvatar.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace sims {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return lower(haystack).find(needle) != std::string::npos;
}

} // namespace

void SimAvatar::init(const std::string& assets_dir) {
    if (mode_ != Mode::Uninit) return;
    // Procedural cube is always built (cheap, and used as the fallback).
    cube_ = Mesh::make_cube(1.0f);

    load_skinned_asset(assets_dir);

    if (mode_ != Mode::Skinned) {
        mode_ = Mode::Procedural;
        std::printf("[avatar] no skinned asset found; using procedural cube humanoid\n");
    }
}

void SimAvatar::load_skinned_asset(const std::string& assets_dir) {
    namespace fs = std::filesystem;
    const fs::path dir = assets_dir + "/models";

    // Try a few common avatar filenames + formats.
    const std::vector<std::string> candidates = {
        "female_avatar.glb", "female_avatar.gltf", "female_avatar.fbx",
        "avatar.glb", "avatar.gltf", "avatar.fbx",
        "sim.glb", "sim.gltf", "sim.fbx",
    };

    fs::path chosen;
    for (const auto& name : candidates) {
        fs::path p = dir / name;
        if (fs::exists(p)) { chosen = p; break; }
    }
    // If nothing matched but the dir exists, try the first .glb/.gltf/.fbx found.
    if (chosen.empty() && fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::string ext = lower(entry.path().extension().string());
            if (ext == ".glb" || ext == ".gltf" || ext == ".fbx") {
                chosen = entry.path();
                break;
            }
        }
    }
    if (chosen.empty()) return;

    if (!skin_shader_.load_from_files(assets_dir + "/shaders/lit_skin.vert",
                                      assets_dir + "/shaders/lit.frag")) {
        std::fprintf(stderr, "[avatar] lit_skin shader failed; falling back to procedural\n");
        return;
    }
    if (!model_.load_from_file(chosen.string())) {
        std::fprintf(stderr, "[avatar] skinned load failed; falling back to procedural\n");
        return;
    }

    // Merge animation clips from sibling files in the same directory. Mixamo
    // ships each clip as its own FBX (all named "mixamo.com" internally), so
    // we override each clip's name with the file's stem — e.g. "Neutral Idle"
    // → matched as idle, "Walking" → walk, "Running" → run. The avatar file
    // itself is skipped (its embedded clips are already loaded).
    namespace fs = std::filesystem;
    if (fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path() == chosen) continue;
            std::string ext = lower(entry.path().extension().string());
            if (ext != ".fbx" && ext != ".glb" && ext != ".gltf") continue;
            std::string stem = entry.path().stem().string();
            model_.load_animations_from_file(entry.path().string(), stem);
        }
    }

    animator_.set_skeleton(&model_.skeleton());
    pick_clips();
    bone_matrices_.assign(model_.skeleton().bones().size(), glm::mat4(1.0f));
    mode_ = Mode::Skinned;
    std::printf("[avatar] skinned mode: idle=\"%s\" walk=\"%s\" run=\"%s\"\n",
                idle_clip_.c_str(), walk_clip_.c_str(), run_clip_.c_str());
}

void SimAvatar::pick_clips() {
    const auto& names = model_.clip_names();
    if (names.empty()) return;
    auto find_if = [&](auto pred) -> std::string {
        for (const auto& n : names) if (pred(n)) return n;
        return {};
    };
    idle_clip_ = find_if([](const std::string& n) {
        return contains(n, "idle") || contains(n, "rest") || contains(n, "stand") || contains(n, "breath");
    });
    walk_clip_ = find_if([](const std::string& n) {
        return contains(n, "walk") || contains(n, "move");
    });
    run_clip_ = find_if([](const std::string& n) {
        return contains(n, "run") || contains(n, "sprint") || contains(n, "jog");
    });
    if (idle_clip_.empty()) idle_clip_ = names.front();
    if (walk_clip_.empty()) walk_clip_ = run_clip_.empty() ? idle_clip_ : run_clip_;
}

void SimAvatar::update(float dt, bool moving) {
    if (mode_ != Mode::Skinned) return;
    const char* desired = moving ? (walk_clip_.empty() ? nullptr : walk_clip_.c_str())
                                 : (idle_clip_.empty() ? nullptr : idle_clip_.c_str());
    if (desired && (animator_.current_clip() != desired || !animator_.playing())) {
        animator_.play(desired, true, 1.0f, false);
    } else if (!desired) {
        animator_.play("", false, 1.0f, false);
    }
    animator_.update(dt);
}

void SimAvatar::draw_part(Shader& lit, const glm::mat4& model, const glm::vec3& color) {
    lit.set_mat4("u_model", &model[0][0]);
    lit.set_vec3("u_base_color", color.r, color.g, color.b);
    lit.set_int("u_has_texture", 0);
    cube_.draw();
}

void SimAvatar::draw_procedural(Shader& lit, const glm::vec3& world_pos,
                                float facing_deg, float walk_phase, bool moving) {
    float yaw = glm::radians(facing_deg);
    float bob = moving ? std::sin(walk_phase * 2.0f) * 0.04f : 0.0f;
    float leg_swing = moving ? std::sin(walk_phase) * 0.5f : 0.0f;
    float arm_swing = moving ? std::sin(walk_phase) * 0.4f : 0.0f;

    glm::vec3 base = world_pos + glm::vec3(0.0f, bob, 0.0f);
    auto root = glm::rotate(glm::translate(glm::mat4(1.0f), base), yaw, glm::vec3(0, 1, 0));

    glm::mat4 torso = glm::scale(
        glm::translate(root, {0.0f, 1.05f, 0.0f}),
        {0.45f, 0.70f, 0.25f});
    draw_part(lit, torso, {0.2f, 0.5f, 0.8f});

    glm::mat4 head = glm::scale(
        glm::translate(root, {0.0f, 1.55f, 0.0f}),
        {0.22f, 0.22f, 0.22f});
    draw_part(lit, head, {0.95f, 0.78f, 0.65f});

    auto leg = [&](float side, float swing) {
        glm::mat4 hip = glm::translate(root, {side * 0.11f, 0.70f, 0.0f});
        hip = glm::rotate(hip, swing, glm::vec3(1, 0, 0));
        glm::mat4 leg_m = glm::scale(
            glm::translate(hip, {0.0f, -0.40f, 0.0f}),
            {0.15f, 0.80f, 0.15f});
        draw_part(lit, leg_m, {0.15f, 0.2f, 0.5f});
    };
    leg(-1.0f, leg_swing);
    leg( 1.0f, -leg_swing);

    auto arm = [&](float side, float swing) {
        glm::mat4 shoulder = glm::translate(root, {side * 0.28f, 1.35f, 0.0f});
        shoulder = glm::rotate(shoulder, swing, glm::vec3(1, 0, 0));
        glm::mat4 arm_m = glm::scale(
            glm::translate(shoulder, {0.0f, -0.27f, 0.0f}),
            {0.12f, 0.55f, 0.12f});
        draw_part(lit, arm_m, {0.2f, 0.5f, 0.8f});
    };
    arm(-1.0f, -arm_swing);
    arm( 1.0f, arm_swing);
}

void SimAvatar::draw_skinned(const glm::mat4& view_proj,
                             const glm::vec3& world_pos, float facing_deg) {
    constexpr int kMaxBones = 128;
    int nb = static_cast<int>(bone_matrices_.size());
    if (nb > kMaxBones) {
        static bool warned = false;
        if (!warned) {
            std::fprintf(stderr, "[avatar] %d bones exceeds shader max %d; clamping\n",
                         nb, kMaxBones);
            warned = true;
        }
        nb = kMaxBones;
    }

    Shader& s = skin_shader_;
    s.use();
    s.set_mat4("u_view_proj", &view_proj[0][0]);
    s.set_vec3("u_light_dir", 0.4f, 0.9f, 0.3f);
    s.set_vec3("u_light_color", 1.2f, 1.15f, 1.05f);
    s.set_vec3("u_ambient", 0.45f, 0.47f, 0.52f);

    float yaw = glm::radians(facing_deg);

    constexpr float kTargetHeight = 1.7f;
    float raw_h = model_.height();
    float scale = (raw_h > 1e-4f) ? kTargetHeight / raw_h : 1.0f;
    float y_off = -model_.bbox_min().y * scale;

    glm::mat4 model = glm::scale(
        glm::rotate(
            glm::translate(glm::mat4(1.0f), world_pos + glm::vec3(0.0f, y_off, 0.0f)),
            yaw, glm::vec3(0, 1, 0)),
        glm::vec3(scale));
    s.set_mat4("u_model", &model[0][0]);

    for (int i = 0; i < nb; ++i) {
        char uname[32];
        std::snprintf(uname, sizeof(uname), "u_bone_matrices[%d]", i);
        s.set_mat4(uname, &bone_matrices_[i][0][0]);
    }

    // One-shot draw-time diagnostic.
    static bool diag = false;
    if (!diag) {
        diag = true;
        std::printf("[avatar-draw] skin prog=%u nbones=%d scale=%.5f y_off=%.4f\n",
                    s.program(), nb, scale, y_off);
        std::printf("[avatar-draw] world_pos=(%.3f,%.3f,%.3f) facing=%.1f\n",
                    world_pos.x, world_pos.y, world_pos.z, facing_deg);
    }

    for (const SkinnedModelMesh& mm : model_.meshes()) {
        if (mm.texture) {
            mm.texture->bind(0);
            s.set_int("u_has_texture", 1);
            glm::vec3 boosted = glm::clamp(
                glm::vec3(mm.base_color) * 1.25f, 0.0f, 1.0f);
            s.set_vec3("u_base_color", boosted.r, boosted.g, boosted.b);
        } else {
            s.set_int("u_has_texture", 0);
            s.set_vec3("u_base_color", 0.9f, 0.3f, 0.3f);
        }
        mm.mesh.draw();
    }
    s.release();
}

void SimAvatar::draw(Shader& lit, const glm::vec3& world_pos,
                     float facing_deg, float walk_phase, bool moving,
                     const glm::mat4& view_proj) {
    if (mode_ == Mode::Skinned) {
        animator_.compute_bone_matrices(bone_matrices_);
        draw_skinned(view_proj, world_pos, facing_deg);
    } else {
        draw_procedural(lit, world_pos, facing_deg, walk_phase, moving);
    }
}

} // namespace sims

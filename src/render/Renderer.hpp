#pragma once

#include "render/Camera.hpp"
#include "render/Mesh.hpp"
#include "render/Model.hpp"
#include "render/Shader.hpp"
#include "render/SimAvatar.hpp"
#include "render/Texture.hpp"
#include "sim/world/TileGrid.hpp"

#include <optional>
#include <string>

namespace sims {

// Sim render state pushed by the Application each frame.
struct SimRenderState {
    glm::vec3 position{0.0f};
    float facing_deg = 0.0f;
    float walk_phase = 0.0f;
    bool moving = false;
};

// Owns the default lit + flat (unlit) shaders, the floor plane, procedural
// primitives, and the wall mesh built from the TileGrid wall set. The
// Application delegates per-frame drawing here.
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    // Loads shaders + builds procedural floor + grid line meshes.
    // Returns false on shader failure.
    bool init(int viewport_w, int viewport_h, const std::string& assets_dir);

    void on_resize(int w, int h);

    // Optional scene state — set by Application before calling render().
    void set_tile_grid(const world::TileGrid* grid) { grid_ = grid; }
    void set_hovered_tile(std::optional<glm::ivec2> tile) { hovered_tile_ = tile; }
    void set_preview_wall(std::optional<world::TileGrid::Edge> e) { preview_wall_ = e; }
    void set_build_mode(bool b) { build_mode_ = b; }
    void set_sim(std::optional<SimRenderState> s) { sim_state_ = s; }
    void set_path(std::vector<glm::vec3> pts) { path_pts_ = std::move(pts); }

    void render(const Camera& cam, double /*alpha*/);

private:
    void rebuild_grid_lines(const world::TileGrid& grid);
    void rebuild_wall_mesh(const world::TileGrid& grid);

    Shader lit_shader_;
    Shader flat_shader_;
    Mesh floor_mesh_;
    Mesh furniture_mesh_;
    Texture floor_texture_;
    SimAvatar avatar_;

    // Per-frame-rebuilt line + wall buffers (Phase 2: lots are small).
    GLuint grid_vao_ = 0;
    GLuint grid_vbo_ = 0;
    GLsizei grid_vert_count_ = 0;

    GLuint wall_vao_ = 0;
    GLuint wall_vbo_ = 0;
    GLuint wall_ibo_ = 0;
    GLsizei wall_index_count_ = 0;

    GLuint path_vao_ = 0;
    GLuint path_vbo_ = 0;
    GLsizei path_vert_count_ = 0;
    std::vector<glm::vec3> path_pts_;

    std::string assets_dir_;
    const world::TileGrid* grid_ = nullptr;
    std::optional<glm::ivec2> hovered_tile_;
    std::optional<world::TileGrid::Edge> preview_wall_;
    std::optional<SimRenderState> sim_state_;
    bool build_mode_ = false;
};

} // namespace sims

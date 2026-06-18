#include "render/Renderer.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <vector>

namespace sims {

namespace {

GLuint make_checker_texture(int w, int h, int cell) {
    std::vector<unsigned char> data(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool light = ((x / cell) + (y / cell)) % 2 == 0;
            unsigned char v = light ? 200 : 90;
            unsigned char* p = &data[(static_cast<size_t>(y) * w + x) * 4];
            p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
        }
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

} // namespace

bool Renderer::init(int viewport_w, int viewport_h, const std::string& assets_dir) {
    assets_dir_ = assets_dir;
    on_resize(viewport_w, viewport_h);

    if (!lit_shader_.load_from_files(assets_dir + "/shaders/lit.vert",
                                     assets_dir + "/shaders/lit.frag")) {
        std::fprintf(stderr, "[renderer] failed to load lit shader\n");
        return false;
    }
    if (!flat_shader_.load_from_files(assets_dir + "/shaders/flat.vert",
                                     assets_dir + "/shaders/flat.frag")) {
        std::fprintf(stderr, "[renderer] failed to load flat shader\n");
        return false;
    }

    floor_mesh_ = Mesh::make_plane(20.0f, 20.0f, 1, 1);
    furniture_mesh_ = Mesh::make_cube(1.0f);
    floor_texture_.adopt(make_checker_texture(256, 256, 32), 256, 256);

    glGenVertexArrays(1, &grid_vao_);
    glGenBuffers(1, &grid_vbo_);
    glGenVertexArrays(1, &wall_vao_);
    glGenBuffers(1, &wall_vbo_);
    glGenBuffers(1, &wall_ibo_);
    glGenVertexArrays(1, &path_vao_);
    glGenBuffers(1, &path_vbo_);
    return true;
}

void Renderer::on_resize(int w, int h) {
    glViewport(0, 0, w, h);
}

void Renderer::rebuild_grid_lines(const world::TileGrid& grid) {
    int W = grid.width();
    int D = grid.depth();
    std::vector<glm::vec3> verts;
    verts.reserve(static_cast<size_t>((W + 1 + D + 1) * 2));
    float x0 = -W * 0.5f;
    float z0 = -D * 0.5f;
    float y = 0.02f; // lift slightly above floor to avoid z-fighting
    for (int x = 0; x <= W; ++x) {
        verts.push_back({x0 + x, y, z0});
        verts.push_back({x0 + x, y, z0 + D});
    }
    for (int z = 0; z <= D; ++z) {
        verts.push_back({x0,     y, z0 + z});
        verts.push_back({x0 + W, y, z0 + z});
    }

    glBindVertexArray(grid_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
    grid_vert_count_ = static_cast<GLsizei>(verts.size());
}

void Renderer::rebuild_wall_mesh(const world::TileGrid& grid) {
    std::vector<glm::vec3> verts;
    std::vector<unsigned int> idx;
    auto push_wall = [&](glm::vec3 a, glm::vec3 b) {
        // a, b are y=0 endpoints. Build a thin box (4 side faces + top).
        constexpr float wall_h = 2.7f;
        constexpr float wall_t = 0.1f;
        glm::vec3 dir = glm::normalize(b - a);
        glm::vec3 perp = glm::normalize(glm::cross(glm::vec3(0, 1, 0), dir)) * (wall_t * 0.5f);
        glm::vec3 up(0, wall_h, 0);
        glm::vec3 a1 = a + perp, a2 = a - perp;
        glm::vec3 b1 = b + perp, b2 = b - perp;
        unsigned int base = static_cast<unsigned int>(verts.size());
        verts.push_back(a1);           // 0
        verts.push_back(b1);           // 1
        verts.push_back(b1 + up);      // 2
        verts.push_back(a1 + up);      // 3
        verts.push_back(a2 + up);      // 4
        verts.push_back(b2 + up);      // 5
        verts.push_back(b2);           // 6
        verts.push_back(a2);           // 7
        auto quad = [&](unsigned int i0, unsigned int i1, unsigned int i2, unsigned int i3) {
            idx.insert(idx.end(), {i0, i1, i2, i0, i2, i3});
        };
        quad(base + 0, base + 1, base + 2, base + 3); // +perp face
        quad(base + 4, base + 5, base + 6, base + 7); // -perp face (reversed winding)
        quad(base + 3, base + 2, base + 5, base + 4); // top
        quad(base + 0, base + 3, base + 4, base + 7); // cap A
        quad(base + 1, base + 6, base + 5, base + 2); // cap B
    };

    for (const auto& e : grid.walls()) {
        glm::vec3 a, b;
        grid.edge_endpoints(e, a, b);
        push_wall(a, b);
    }
    // Add preview wall (drawn in this same pass; tint handled in render).
    if (preview_wall_ && !grid.has_wall(*preview_wall_) && grid.in_bounds_edge(*preview_wall_)) {
        glm::vec3 a, b;
        grid.edge_endpoints(*preview_wall_, a, b);
        push_wall(a, b);
    }

    glBindVertexArray(wall_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, wall_vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wall_ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);
    wall_index_count_ = static_cast<GLsizei>(idx.size());
}

void Renderer::render(const Camera& cam, double /*alpha*/) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 vp = cam.view_projection();
    glm::mat4 identity(1.0f);

    // --- Lit pass: floor + furniture cube ----------------------------------
    lit_shader_.use();
    lit_shader_.set_mat4("u_view_proj", &vp[0][0]);
    lit_shader_.set_vec3("u_light_dir", 0.4f, 0.9f, 0.3f);
    lit_shader_.set_vec3("u_light_color", 1.0f, 0.96f, 0.88f);
    lit_shader_.set_vec3("u_ambient", 0.25f, 0.27f, 0.32f);

    if (grid_) {
        // Scale floor plane to cover the lot exactly.
        float w = static_cast<float>(grid_->width());
        float d = static_cast<float>(grid_->depth());
        glm::mat4 fm = glm::scale(glm::translate(glm::mat4(1.0f), {0, 0, 0}), {w, 1, d});
        lit_shader_.set_mat4("u_model", &fm[0][0]);
    } else {
        lit_shader_.set_mat4("u_model", &identity[0][0]);
    }
    lit_shader_.set_vec3("u_base_color", 0.9f, 0.9f, 0.9f);
    lit_shader_.set_int("u_has_texture", 1);
    floor_texture_.bind(0);
    floor_mesh_.draw();

    // Sim avatar (procedural humanoid with walk-cycle animation).
    if (sim_state_) {
        avatar_.draw(lit_shader_, sim_state_->position, sim_state_->facing_deg,
                     sim_state_->walk_phase, sim_state_->moving);

        static bool verified = false;
        if (!verified) {
            GLint vp[4];
            glGetIntegerv(GL_VIEWPORT, vp);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            int blue_hits = 0;
            for (int oy = -30; oy <= 30; oy += 6) {
                for (int ox = -30; ox <= 30; ox += 6) {
                    unsigned char px[3];
                    glReadPixels(vp[2]/2 + ox, vp[3]/2 + oy, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, px);
                    if (px[2] > px[0] + 25 && px[2] > px[1] + 10) blue_hits++;
                }
            }
            std::printf("[render] avatar sim_pos=(%.2f,%.2f,%.2f) blue-ish pixels near center: %d/121\n",
                sim_state_->position.x, sim_state_->position.y, sim_state_->position.z, blue_hits);
            verified = true;
        }
    }
    lit_shader_.release();

    // --- Flat pass: grid lines + walls + hovered tile -----------------------
    if (grid_) {
        rebuild_grid_lines(*grid_);
        rebuild_wall_mesh(*grid_);

        flat_shader_.use();
        flat_shader_.set_mat4("u_view_proj", &vp[0][0]);

        // Grid lines.
        flat_shader_.set_mat4("u_model", &identity[0][0]);
        if (build_mode_) {
            flat_shader_.set_vec3("u_color", 0.5f, 0.9f, 0.6f);
        } else {
            flat_shader_.set_vec3("u_color", 0.3f, 0.3f, 0.3f);
        }
        glBindVertexArray(grid_vao_);
        glDrawArrays(GL_LINES, 0, grid_vert_count_);
        glBindVertexArray(0);

        // Walls (solid + preview combined in one mesh; tint all the same for Phase 2).
        glm::mat4 wall_model(1.0f);
        flat_shader_.set_mat4("u_model", &wall_model[0][0]);
        if (build_mode_) {
            flat_shader_.set_vec3("u_color", 0.7f, 0.7f, 0.75f);
        } else {
            flat_shader_.set_vec3("u_color", 0.85f, 0.82f, 0.78f);
        }
        glBindVertexArray(wall_vao_);
        glDrawElements(GL_TRIANGLES, wall_index_count_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        // Path waypoints (debug line strip) — Phase 4.
        if (path_pts_.size() >= 2) {
            glBindVertexArray(path_vao_);
            glBindBuffer(GL_ARRAY_BUFFER, path_vbo_);
            glBufferData(GL_ARRAY_BUFFER, path_pts_.size() * sizeof(glm::vec3),
                         path_pts_.data(), GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
            flat_shader_.set_mat4("u_model", &identity[0][0]);
            flat_shader_.set_vec3("u_color", 0.2f, 0.9f, 1.0f);
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(path_pts_.size()));
            glBindVertexArray(0);
        }

        flat_shader_.release();
    }
}

} // namespace sims

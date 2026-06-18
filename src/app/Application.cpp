#include "app/Application.hpp"

#include "sim/motives/Motives.hpp"

#include <glad/gl.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <SDL2/SDL.h>

#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace sims {

Application::Application(AppConfig cfg) : cfg_(std::move(cfg)) {}

Application::~Application() { shutdown(); }

bool Application::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    window_ = SDL_CreateWindow(
        cfg_.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg_.width, cfg_.height, flags);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    gl_ = SDL_GL_CreateContext(window_);
    if (!gl_) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    if (SDL_GL_SetSwapInterval(cfg_.vsync ? 1 : 0) != 0) {
        std::fprintf(stderr, "VSync set failed (non-fatal): %s\n", SDL_GetError());
    }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        std::fprintf(stderr, "gladLoadGL failed\n");
        return false;
    }
    std::printf("[render] OpenGL %s, GLSL %s\n",
        glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window_, gl_);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window_, &drawable_w, &drawable_h);
    const char* assets_dir_env = getenv("SIMS_ASSETS_DIR");
    std::string assets_dir = assets_dir_env ? assets_dir_env : SIMS_ASSETS_DIR;
    if (!renderer_.init(drawable_w, drawable_h, assets_dir)) {
        std::fprintf(stderr, "[app] renderer init failed\n");
        return false;
    }
    camera_.set_perspective(50.0f, static_cast<float>(drawable_w) / drawable_h, 0.1f, 200.0f);
    camera_.set_orbit(45.0f, 35.0f, 12.0f);
    camera_.set_target({0.0f, 1.0f, 0.0f});

    renderer_.set_tile_grid(&grid_);

    pathfinder_.emplace(grid_);

    // Seed a small 4x4 starter room outline (tiles 6..9 on each axis) so the
    // wall renderer has visible geometry before the user places anything.
    for (int i = 0; i < 4; ++i) {
        grid_.add_wall(6 + i, 6, world::TileGrid::Side::North);  // bottom edge (z=6)
        grid_.add_wall(6 + i, 9, world::TileGrid::Side::North);  // top edge (z=10 line)
        grid_.add_wall(6, 6 + i, world::TileGrid::Side::East);   // left edge (x=6)
        grid_.add_wall(9, 6 + i, world::TileGrid::Side::East);   // right edge (x=10 line)
    }

    // Create the Sim entity at the center of the starter room (tile 8,8).
    glm::vec3 sim_start = grid_.coord().tile_to_world(8, 8);
    sim_entity_ = registry_.create();
    registry_.emplace<Transform>(sim_entity_, Transform{sim_start, 0.0f, {1, 1, 1}});
    registry_.emplace<Sim>(sim_entity_, Sim{"Alex", 0.0f, 0.0f, false});
    registry_.emplace<Movement>(sim_entity_, Movement{sim_start, sim_start, 2.0f, false, {}});
    registry_.emplace<Motives>(sim_entity_, Motives{});

    return true;
}

void Application::shutdown() {
    if (gl_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_);
        gl_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void Application::handle_event(const SDL_Event& ev) {
    ImGui_ImplSDL2_ProcessEvent(&ev);
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse && (ev.type == SDL_MOUSEBUTTONDOWN ||
        ev.type == SDL_MOUSEBUTTONUP || ev.type == SDL_MOUSEWHEEL ||
        ev.type == SDL_MOUSEMOTION)) return;
    if (io.WantCaptureKeyboard && (ev.type == SDL_KEYDOWN ||
        ev.type == SDL_KEYUP || ev.type == SDL_TEXTINPUT)) return;

    switch (ev.type) {
        case SDL_QUIT: running_ = false; break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_CLOSE) running_ = false;
            else if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                int w = 0, h = 0;
                SDL_GL_GetDrawableSize(window_, &w, &h);
                renderer_.on_resize(w, h);
                camera_.set_perspective(50.0f, static_cast<float>(w) / h, 0.1f, 200.0f);
            }
            break;
        case SDL_KEYDOWN:
            switch (ev.key.keysym.scancode) {
                case SDL_SCANCODE_ESCAPE: running_ = false; break;
                case SDL_SCANCODE_F1: show_demo_ = !show_demo_; break;
                case SDL_SCANCODE_B: build_mode_ = !build_mode_; break;
                default: break;
            }
            break;
        case SDL_MOUSEMOTION:
            mouse_x_ = ev.motion.x;
            mouse_y_ = ev.motion.y;
            if (camera_active_) {
                float dx = static_cast<float>(ev.motion.x - last_mouse_x_);
                float dy = static_cast<float>(ev.motion.y - last_mouse_y_);
                last_mouse_x_ = ev.motion.x;
                last_mouse_y_ = ev.motion.y;
                camera_.rotate(dx * 0.25f, -dy * 0.25f);
            } else if (panning_) {
                float dx = static_cast<float>(ev.motion.x - last_mouse_x_);
                float dy = static_cast<float>(ev.motion.y - last_mouse_y_);
                last_mouse_x_ = ev.motion.x;
                last_mouse_y_ = ev.motion.y;
                camera_.pan(dx, dy);
            }
            break;
        case SDL_MOUSEWHEEL:
            camera_.dolly(-static_cast<float>(ev.wheel.y));
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_MIDDLE) {
                camera_active_ = true;
                last_mouse_x_ = ev.button.x;
                last_mouse_y_ = ev.button.y;
            } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                panning_ = true;
                last_mouse_x_ = ev.button.x;
                last_mouse_y_ = ev.button.y;
            } else if (ev.button.button == SDL_BUTTON_LEFT && build_mode_) {
                // Toggle wall on the hovered edge (left-click placement).
                update_hover();
                if (hovered_edge_ && grid_.in_bounds_edge(*hovered_edge_)) {
                    if (grid_.has_wall(*hovered_edge_)) grid_.remove_wall(*hovered_edge_);
                    else grid_.add_wall(*hovered_edge_);
                }
            } else if (ev.button.button == SDL_BUTTON_LEFT && !build_mode_) {
                // Live mode: click-to-walk via A* pathfinding.
                int w = 0, h = 0;
                SDL_GL_GetDrawableSize(window_, &w, &h);
                auto ground = camera_.screen_to_ground(ev.button.x, ev.button.y, w, h);
                if (ground && sim_entity_ != entt::null && pathfinder_) {
                    int stx = 0, stz = 0;
                    auto* tr = registry_.try_get<Transform>(sim_entity_);
                    if (tr && grid_.coord().world_to_tile(tr->position, stx, stz)) {
                        int gtx = 0, gtz = 0;
                        if (grid_.coord().world_to_tile(*ground, gtx, gtz)) {
                            auto path = pathfinder_->find({stx, stz}, {gtx, gtz});
                            last_path_ = path;
                            if (path.valid) {
                                MovementSystem::set_path(registry_, sim_entity_, path,
                                                         grid_.coord());
                            }
                        }
                    }
                }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_MIDDLE) camera_active_ = false;
            else if (ev.button.button == SDL_BUTTON_RIGHT) panning_ = false;
            break;
    }
}

void Application::update_hover() {
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window_, &w, &h);
    auto ground = camera_.screen_to_ground(mouse_x_, mouse_y_, w, h);
    hovered_tile_.reset();
    hovered_edge_.reset();
    if (!ground) return;

    int tx = 0, tz = 0;
    if (!grid_.coord().world_to_tile(*ground, tx, tz)) return;
    hovered_tile_ = glm::ivec2{tx, tz};

    // Fractional position within the tile (0..1 on each axis).
    float fx = ground->x - (tx - grid_.width() * 0.5f);
    float fz = ground->z - (tz - grid_.depth() * 0.5f);
    // Distances to the 4 edges; pick closest. Canonicalize to East/North.
    float dE = 1.0f - fx; // +x edge of (tx, tz)
    float dW = fx;        // -x edge → East edge of (tx-1, tz)
    float dN = 1.0f - fz; // +z edge of (tx, tz)
    float dS = fz;        // -z edge → North edge of (tx, tz-1)
    float best = dE;
    world::TileGrid::Edge pick{tx, tz, world::TileGrid::Side::East};
    if (dW < best) { best = dW; pick = {tx - 1, tz, world::TileGrid::Side::East}; }
    if (dN < best) { best = dN; pick = {tx, tz, world::TileGrid::Side::North}; }
    if (dS < best) { best = dS; pick = {tx, tz - 1, world::TileGrid::Side::North}; }
    hovered_edge_ = pick;
}

void Application::step_sim(double dt) {
    // Advance sim-time and tick the motives system. dt is in real seconds;
    // the clock converts to sim-minutes via its time scale.
    clock_.tick(dt);
    const double sim_minutes = dt * clock_.minutes_per_second();
    motive_system_.update(registry_, sim_minutes);

    // Phase 3: advance Sim movement each fixed tick.
    if (sim_entity_ != entt::null) {
        movement_.update(registry_, static_cast<float>(dt));
    }
}

void Application::render(double /*alpha*/) {
    int w, h;
    SDL_GL_GetDrawableSize(window_, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderer_.render(camera_, 0.0);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", "F1", &show_demo_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Esc")) running_ = false;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Sim Debug")) {
        ImGui::TextUnformatted("Phase 5: Motives + SimClock.");
        ImGui::Separator();
        char clock_buf[32];
        clock_.format(clock_buf, sizeof(clock_buf));
        ImGui::Text("Sim time: %s  (%.1f min/sec)", clock_buf, clock_.minutes_per_second());
        if (ImGui::Button("x0.5")) clock_.set_minutes_per_second(0.5);
        ImGui::SameLine();
        if (ImGui::Button("x1"))   clock_.set_minutes_per_second(1.0);
        ImGui::SameLine();
        if (ImGui::Button("x5"))   clock_.set_minutes_per_second(5.0);
        ImGui::SameLine();
        if (ImGui::Button("x20"))  clock_.set_minutes_per_second(20.0);
        ImGui::Separator();
        ImGui::Text("Window: %dx%d", w, h);
        ImGui::Text("Camera yaw=%.1f pitch=%.1f dist=%.1f",
            camera_.yaw(), camera_.pitch(), camera_.distance());
        ImGui::Separator();
        ImGui::Text("Mode: %s", build_mode_ ? "BUILD (B to toggle)" : "LIVE (B for build)");
        ImGui::Text("Grid: %dx%d  Walls: %zu", grid_.width(), grid_.depth(), grid_.wall_count());

        if (sim_entity_ != entt::null) {
            auto* tr = registry_.try_get<Transform>(sim_entity_);
            auto* sim = registry_.try_get<Sim>(sim_entity_);
            auto* mv = registry_.try_get<Movement>(sim_entity_);
            auto* mo = registry_.try_get<Motives>(sim_entity_);
            if (tr && sim && mv) {
                ImGui::Separator();
                ImGui::Text("Sim: %s  %s", sim->name.c_str(), sim->moving ? "moving" : "idle");
                ImGui::Text("Pos: (%.2f, %.2f, %.2f)", tr->position.x, tr->position.y, tr->position.z);
                ImGui::Text("Facing: %.1f deg", sim->facing_deg);
                if (mv->has_target) {
                    ImGui::Text("Target: (%.2f, %.2f, %.2f)", mv->target.x, mv->target.y, mv->target.z);
                    ImGui::Text("Waypoints remaining: %zu", mv->waypoints.size() + 1);
                }
            }
            if (mo) {
                ImGui::Separator();
                ImGui::TextUnformatted("Motives:");
                for (std::size_t i = 0; i < Motives::kCount; ++i) {
                    float v = mo->value[i];
                    ImGui::Text("  %-8s %5.1f  ", Motives::kNames[i], v);
                    ImGui::SameLine();
                    ImU32 bar = IM_COL32(0, 0, 0, 0);
                    if (v < 20.0f)      bar = IM_COL32(220, 60, 60, 255);
                    else if (v < 50.0f) bar = IM_COL32(220, 180, 60, 255);
                    else                bar = IM_COL32(80, 200, 110, 255);
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(
                        ((bar >> 0) & 0xFF) / 255.0f,
                        ((bar >> 8) & 0xFF) / 255.0f,
                        ((bar >> 16) & 0xFF) / 255.0f,
                        1.0f));
                    char id[16];
                    std::snprintf(id, sizeof(id), "##m%zu", i);
                    ImGui::ProgressBar(v / 100.0f, ImVec2(120.0f, 0.0f), id);
                    ImGui::PopStyleColor();
                }
                ImGui::Text("Mood (avg): %.1f", mo->average());
            }
        }

        if (last_path_ && last_path_->valid) {
            ImGui::Separator();
            ImGui::Text("Last path: %zu tiles", last_path_->size());
        } else if (last_path_) {
            ImGui::Separator();
            ImGui::TextUnformatted("Last path: BLOCKED (no route)");
        }

        if (hovered_tile_) {
            ImGui::Text("Hover tile: (%d, %d)", hovered_tile_->x, hovered_tile_->y);
        }
        if (hovered_edge_) {
            const char* s = hovered_edge_->side == world::TileGrid::Side::East ? "E" : "N";
            ImGui::Text("Hover edge: (%d, %d, %s)", hovered_edge_->tile_x, hovered_edge_->tile_z, s);
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Controls:");
        ImGui::BulletText("Live mode: left-click sets walk target");
        ImGui::BulletText("B: toggle build mode");
        ImGui::BulletText("Build mode: left-click places/removes wall");
        ImGui::BulletText("Middle-drag: orbit");
        ImGui::BulletText("Right-drag: pan");
        ImGui::BulletText("Wheel: dolly");
        ImGui::BulletText("F1: ImGui demo   Esc: quit");
        ImGui::End();
    }

    if (show_demo_) ImGui::ShowDemoWindow(&show_demo_);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
}

int Application::run() {
    if (!init()) return 1;
    running_ = true;

    using clock = std::chrono::steady_clock;
    const double sim_dt = 1.0 / static_cast<double>(cfg_.sim_hz);
    auto last = clock::now();
    double accum = 0.0;

    while (running_) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) handle_event(ev);

        const auto now = clock::now();
        const double frame_dt = std::chrono::duration<double>(now - last).count();
        last = now;

        // Fixed-step accumulator; cap to avoid spiral-of-death after stalls.
        accum += std::min(frame_dt, 0.25);
        while (accum >= sim_dt) {
            step_sim(sim_dt);
            accum -= sim_dt;
        }

        if (build_mode_) update_hover();
        else { hovered_tile_.reset(); hovered_edge_.reset(); }

        renderer_.set_build_mode(build_mode_);
        renderer_.set_hovered_tile(hovered_tile_);
        renderer_.set_preview_wall(build_mode_ ? hovered_edge_ : std::nullopt);

        // Build the live path polyline: current Sim position + remaining
        // waypoints (target + deque). Cleared when the Sim is idle.
        std::vector<glm::vec3> path_pts;
        if (sim_entity_ != entt::null) {
            auto* tr = registry_.try_get<Transform>(sim_entity_);
            auto* mv = registry_.try_get<Movement>(sim_entity_);
            if (tr && mv && mv->has_target) {
                path_pts.push_back({tr->position.x, 0.05f, tr->position.z});
                path_pts.push_back({mv->target.x, 0.05f, mv->target.z});
                for (const auto& wp : mv->waypoints) {
                    path_pts.push_back({wp.x, 0.05f, wp.z});
                }
            }
        }
        renderer_.set_path(std::move(path_pts));

        if (sim_entity_ != entt::null) {
            auto* tr = registry_.try_get<Transform>(sim_entity_);
            auto* sim = registry_.try_get<Sim>(sim_entity_);
            auto* mv = registry_.try_get<Movement>(sim_entity_);
            if (tr && sim && mv) {
                SimRenderState s;
                s.position = tr->position;
                s.facing_deg = sim->facing_deg;
                s.walk_phase = sim->walk_phase;
                s.moving = sim->moving;
                renderer_.set_sim(s);
            }
        } else {
            renderer_.set_sim(std::nullopt);
        }

        render(accum / sim_dt); // alpha in [0,1); unused in Phase 2
    }

    return 0;
}

} // namespace sims

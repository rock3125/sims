#pragma once

#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "sim/ActionSystem.hpp"
#include "sim/MotiveSystem.hpp"
#include "sim/MovementSystem.hpp"
#include "sim/pathfinding/Pathfinder.hpp"
#include "sim/time/SimClock.hpp"
#include "sim/world/TileGrid.hpp"
#include "content/InteractionLibrary.hpp"
#include "ecs/registry.hpp"
#include "ui/PieMenu.hpp"

#include <SDL2/SDL.h>

#include <optional>
#include <string>

namespace sims {

struct AppConfig {
    std::string title = "sims";
    int width = 1280;
    int height = 720;
    int sim_hz = 60;            // fixed simulation steps per second
    bool vsync = true;
};

// Owns the SDL window + GL context, drives the fixed-step main loop,
// and bridges input/rendering to subsystems added in later phases.
class Application {
public:
    explicit Application(AppConfig cfg = {});
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    bool init();
    void shutdown();
    void handle_event(const SDL_Event& ev);
    void update_hover();
    void step_sim(double dt);   // fixed-step simulation tick
    void render(double alpha);  // interpolated frame render

    // Phase 6 helpers
    Entity pick_object(int screen_x, int screen_y);
    void open_pie_menu_for(Entity obj_e, int screen_x, int screen_y);

    AppConfig cfg_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_ = nullptr;
    bool running_ = false;
    bool show_demo_ = false;

    // Phase 1: renderer + camera
    Renderer renderer_;
    Camera camera_;
    bool camera_active_ = false; // middle-mouse drag rotates
    bool panning_ = false;       // right-mouse drag pans target
    int last_mouse_x_ = 0;
    int last_mouse_y_ = 0;
    int mouse_x_ = 0;
    int mouse_y_ = 0;

    // Phase 2: tile world + build mode
    world::TileGrid grid_{16, 16};
    bool build_mode_ = false;
    std::optional<glm::ivec2> hovered_tile_;
    std::optional<world::TileGrid::Edge> hovered_edge_;

    // Phase 3: Sim entity + movement
    Registry registry_;
    Entity sim_entity_{entt::null};
    MovementSystem movement_;

    // Phase 4: pathfinding (recomputed on each click-to-walk)
    std::optional<pathfinding::Pathfinder> pathfinder_;
    std::optional<pathfinding::Path> last_path_;

    // Phase 5: sim-time clock + motives
    time::SimClock clock_;
    MotiveSystem motive_system_;
    double sim_minutes_accum_ = 0.0; // sim-minutes elapsed this tick, fed to systems

    // Phase 6: content library, action system, world objects, pie menu
    content::InteractionLibrary library_;
    std::optional<ActionSystem> action_system_;
    ui::PieMenu pie_menu_;
    Entity pie_target_object_{entt::null};
    Entity hovered_object_{entt::null};
};

} // namespace sims

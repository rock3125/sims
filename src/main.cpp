#include "app/Application.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    sims::AppConfig cfg;
    cfg.title = "sims";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.sim_hz = 60;
    cfg.vsync = true;

    sims::Application app(cfg);
    return app.run();
}

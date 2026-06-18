cmake_minimum_required(VERSION 3.20)
project(sims LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

option(SIMS_BUILD_TESTS "Build unit tests" ON)
option(SIMS_ENABLE_CI "CI build (no runtime display required)" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# -------------------------------------------------------------------
# glm
# -------------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glm)

# -------------------------------------------------------------------
# enTT (header-only)
# -------------------------------------------------------------------
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(entt)

# -------------------------------------------------------------------
# nlohmann/json (header-only)
# -------------------------------------------------------------------
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(json)

# -------------------------------------------------------------------
# glad2 (OpenGL loader generator; Python-based, no root CMakeLists)
# -------------------------------------------------------------------
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glad)
find_package(Python COMPONENTS Interpreter REQUIRED)
set(GLAD_SOURCES_DIR "${glad_SOURCE_DIR}")
include("${glad_SOURCE_DIR}/cmake/GladConfig.cmake")
glad_add_library(glad_gl_core_33 STATIC REPRODUCIBLE
    API gl:core=3.3 EXTENSIONS GL_KHR_debug)

# -------------------------------------------------------------------
# Dear ImGui (with SDL2 + OpenGL3 backends)
# -------------------------------------------------------------------
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
    GIT_SHALLOW TRUE)
FetchContent_Populate(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(imgui PUBLIC glad_gl_core_33)

# -------------------------------------------------------------------
# SDL2 (system)
# -------------------------------------------------------------------
find_package(SDL2 REQUIRED)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

# -------------------------------------------------------------------
# stb (header-only image loader)
# -------------------------------------------------------------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE)
FetchContent_Populate(stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

# -------------------------------------------------------------------
# Assimp (system) — model loading
# -------------------------------------------------------------------
find_package(assimp REQUIRED)

# -------------------------------------------------------------------
# Main application
# -------------------------------------------------------------------
add_executable(sims
    src/main.cpp
    src/app/Application.cpp
    src/render/Shader.cpp
    src/render/Texture.cpp
    src/render/Mesh.cpp
    src/render/Model.cpp
    src/render/Camera.cpp
    src/render/Renderer.cpp
    src/render/SimAvatar.cpp
    src/render/stb_image_impl.cpp
    src/sim/MovementSystem.cpp
    src/sim/MotiveSystem.cpp
    src/sim/ActionSystem.cpp
    src/sim/AutonomySystem.cpp
    src/sim/SaveLoad.cpp
    src/sim/pathfinding/Pathfinder.cpp
    src/sim/time/SimClock.cpp
    src/sim/motives/Motives.cpp
    src/content/InteractionLibrary.cpp
    src/audio/AudioSystem.cpp
    src/ui/PieMenu.cpp
)
target_include_directories(sims PRIVATE src)
target_link_libraries(sims PRIVATE
    glad_gl_core_33
    glm::glm
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    imgui
    SDL2::SDL2
    stb
    assimp::assimp
)
target_compile_definitions(sims PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    SIMS_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# -------------------------------------------------------------------
# Tests (Catch2) — added in later phases
# -------------------------------------------------------------------
if(SIMS_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.6.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    add_subdirectory(tests)
endif()

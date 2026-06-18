add_executable(sims_tests
    test_pathfinder.cpp
    test_motives.cpp
    test_simclock.cpp
    test_action_queue.cpp
)
target_include_directories(sims_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(sims_tests PRIVATE
    Catch2::Catch2WithMain
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)

# Link against the pathfinder object sources directly — the engine target
# `sims` is an executable with a main(), so we compile the small Pathfinder
# translation unit into the test binary too. To avoid symbol clashes we keep
# the Pathfinder self-contained (no global state).
target_sources(sims_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/sim/pathfinding/Pathfinder.cpp
    ${CMAKE_SOURCE_DIR}/src/sim/MotiveSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/sim/MovementSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/sim/ActionSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/sim/motives/Motives.cpp
    ${CMAKE_SOURCE_DIR}/src/sim/time/SimClock.cpp
    ${CMAKE_SOURCE_DIR}/src/content/InteractionLibrary.cpp
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(sims_tests)

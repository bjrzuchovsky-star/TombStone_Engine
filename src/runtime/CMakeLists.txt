# Game runtime: World (actors, fixed-step simulation, follow camera, draw
# list) and PlaySession (play / pause / step / stop). Depends only on the
# scene data -- no ImGui, no GL -- so the editor, ts_game and a future
# headless server can all drive it.
add_library(ts_runtime
  World.cpp
  PlaySession.cpp
)

target_include_directories(ts_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_runtime
  PUBLIC
    ts_scene
)

target_compile_features(ts_runtime PUBLIC cxx_std_20)

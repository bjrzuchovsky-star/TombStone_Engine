add_executable(ts_game
  main.cpp
)

target_link_libraries(ts_game
  PRIVATE
    ts_core
    ts_platform
    ts_render
    ts_input
    ts_net
    ts_physics
    ts_scene
)

target_compile_features(ts_game PRIVATE cxx_std_20)

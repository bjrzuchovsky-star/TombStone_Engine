add_executable(ts_admin
  main.cpp
)

target_link_libraries(ts_admin
  PRIVATE
    ts_core
    ts_platform
    ts_render
    ts_input
    ts_net
    ts_physics
    ts_scene
)

target_compile_features(ts_admin PRIVATE cxx_std_20)

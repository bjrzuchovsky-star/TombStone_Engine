add_executable(ts_client
  main.cpp
)

target_link_libraries(ts_client
  PRIVATE
    ts_core
    ts_platform
    ts_render
    ts_input
    ts_net
    ts_physics
    ts_scene
)

target_compile_features(ts_client PRIVATE cxx_std_20)

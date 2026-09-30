add_library(ts_physics
  Physics2D.cpp
)

target_include_directories(ts_physics
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_physics
  PUBLIC
    ts_core
)

target_compile_features(ts_physics PUBLIC cxx_std_20)

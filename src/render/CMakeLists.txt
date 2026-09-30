add_library(ts_render
  Renderer.cpp
)

target_include_directories(ts_render
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_render
  PUBLIC
    ts_core
)

target_compile_features(ts_render PUBLIC cxx_std_20)

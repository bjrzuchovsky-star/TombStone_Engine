add_library(ts_scene
  Scene.cpp
)

target_include_directories(ts_scene
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_scene
  PUBLIC
    ts_core
)

target_compile_features(ts_scene PUBLIC cxx_std_20)

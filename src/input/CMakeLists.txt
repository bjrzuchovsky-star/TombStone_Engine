add_library(ts_input
  Input.cpp
)

target_include_directories(ts_input
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_input
  PUBLIC
    ts_core
)

target_compile_features(ts_input PUBLIC cxx_std_20)

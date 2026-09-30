add_library(ts_net
  Net.cpp
)

target_include_directories(ts_net
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_net
  PUBLIC
    ts_core
)

target_compile_features(ts_net PUBLIC cxx_std_20)

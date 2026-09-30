add_library(ts_editor
  AppFlow.cpp
  screens/LoadingScreen.cpp
  screens/LoginScreen.cpp
  screens/ProjectManagerScreen.cpp
  screens/Editor2DScreen.cpp
)

target_include_directories(ts_editor
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_editor
  PUBLIC
    ts_core
)

target_compile_features(ts_editor PUBLIC cxx_std_20)

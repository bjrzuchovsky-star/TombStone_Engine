add_library(ts_editor
  AppFlow.cpp
  settings/Settings.cpp
  projects/ProjectStore.cpp
  workspace/Workspace2D.cpp
  workspace/SceneIO_Load.cpp
  workspace/SceneIO_Save.cpp
  ui/FolderBrowser.cpp
  screens/LoadingScreen.cpp
  screens/LoginScreen.cpp
  screens/ProjectManagerScreen.cpp
  screens/SettingsScreen.cpp
  screens/Editor2DScreen.cpp
  screens/Editor2DScreen_Panels.cpp
)

target_include_directories(ts_editor
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_editor
  PUBLIC
    ts_core
    ts_imgui
)

target_compile_features(ts_editor PUBLIC cxx_std_20)

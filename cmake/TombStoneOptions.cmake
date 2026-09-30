# TombStone Engine build options

option(TS_BUILD_ADMIN  "Build the Admin (editor/tools) application" ON)
option(TS_BUILD_CLIENT "Build the Client (developer runtime) application" ON)
option(TS_BUILD_GAME   "Build the Game (shipped runtime) application" ON)

message(STATUS "TombStoneEngine options:")
message(STATUS "  TS_BUILD_ADMIN  = ${TS_BUILD_ADMIN}")
message(STATUS "  TS_BUILD_CLIENT = ${TS_BUILD_CLIENT}")
message(STATUS "  TS_BUILD_GAME   = ${TS_BUILD_GAME}")
if(TS_BUILD_ADMIN)
  message(STATUS "  Admin will FetchContent GLFW + Dear ImGui (docking)")
endif()

cmake_minimum_required(VERSION 3.20)
project(TombStoneEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TombStoneOptions)

add_subdirectory(src/core)
add_subdirectory(src/platform)
add_subdirectory(src/render)
add_subdirectory(src/input)
add_subdirectory(src/net)
add_subdirectory(src/physics)
add_subdirectory(src/scene)
# Game runtime (World, fixed-step PlaySession). Pure C++, always built.
add_subdirectory(src/runtime)

# Windowed apps: GLFW + OpenGL + stb_image for ts_admin and ts_game (ts_gfx:
# texture cache + GL helpers). Dear ImGui is fetched only for the Admin.
# Client builds stay free of all of it.
if(TS_BUILD_ADMIN OR TS_BUILD_GAME)
  include(FetchImGuiDeps)
  add_subdirectory(src/gfx)
endif()

# Editor / Admin UI module (screens + AppFlow). Admin-only.
if(TS_BUILD_ADMIN)
  add_subdirectory(src/editor)
  add_subdirectory(apps/admin)
endif()

if(TS_BUILD_CLIENT)
  add_subdirectory(apps/client)
endif()

if(TS_BUILD_GAME)
  add_subdirectory(apps/game)
endif()

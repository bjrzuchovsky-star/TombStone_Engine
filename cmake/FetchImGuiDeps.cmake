# Fetch GLFW + Dear ImGui (docking) for the Admin editor only.
# Sources are downloaded into the CMake build tree (_deps); they are NOT
# committed to this repository. Client/Game builds never include this file.

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# Prefer X11 on Linux; Wayland needs wayland-scanner at configure time.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE
)

# Docking branch tip kept lean via shallow clone. Pin a v*-docking tag later
# if you need bit-identical dependency trees across machines.
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        docking
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

# imgui has no library target of its own; build core + GLFW/OpenGL3 backends.
# imgui_impl_opengl3 embeds a small GL loader (no separate glad required).
add_library(ts_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(ts::imgui ALIAS ts_imgui)

target_include_directories(ts_imgui
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

find_package(OpenGL REQUIRED)

target_link_libraries(ts_imgui
  PUBLIC
    glfw
    OpenGL::GL
)

target_compile_features(ts_imgui PUBLIC cxx_std_20)
target_compile_definitions(ts_imgui
  PUBLIC
    TS_HAS_IMGUI=1
)

# Windows: glfw links the right system libs via its own CMake.
# Linux: needs X11 + OpenGL development packages at configure time.

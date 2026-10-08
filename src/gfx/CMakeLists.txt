# Rendering helpers shared by ts_admin and ts_game: stb_image decode +
# texture cache (renderer-agnostic) and the OpenGL texture uploader. Built
# only when an app that opens a window is enabled (GLFW + stb_image come
# from cmake/FetchImGuiDeps.cmake).
add_library(ts_gfx
  TextureCache.cpp
  StbImageImpl.cpp
  GlTextureUploader.cpp
)

target_include_directories(ts_gfx
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_gfx
  PUBLIC
    ts_core
  PRIVATE
    ts_stb_image
    glfw
    OpenGL::GL
)

target_compile_features(ts_gfx PUBLIC cxx_std_20)

if(MSVC)
  target_compile_definitions(ts_gfx PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

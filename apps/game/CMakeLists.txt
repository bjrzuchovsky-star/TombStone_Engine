# ts_game: the shipped runtime. Window + fixed-function OpenGL renderer over
# ts_runtime; shares GLFW / stb_image (via ts_gfx) with ts_admin, no ImGui.
add_executable(ts_game
  main.cpp
  GameRenderer.cpp
)

target_link_libraries(ts_game
  PRIVATE
    ts_core
    ts_scene
    ts_runtime
    ts_gfx
    glfw
    OpenGL::GL
)

target_compile_features(ts_game PRIVATE cxx_std_20)

if(MSVC)
  target_compile_definitions(ts_game PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

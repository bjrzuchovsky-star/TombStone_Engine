# Networking: byte packing, the Transport interface, its ENet backend and a
# link simulator (latency / jitter / loss). No game code and no window: the
# server, the client and the editor build on top of it.
add_library(ts_net
  ByteStream.cpp
  EnetTransport.cpp
  Net.cpp
  SimTransport.cpp
)

target_include_directories(ts_net
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ts_net
  PUBLIC
    ts_core
  PRIVATE
    ts_enet
)

target_compile_features(ts_net PUBLIC cxx_std_20)

if(MSVC)
  target_compile_definitions(ts_net PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

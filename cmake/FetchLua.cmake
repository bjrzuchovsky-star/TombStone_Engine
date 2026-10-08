# Lua 5.4.8 for gameplay scripts (ts_runtime -> ts_admin Play mode + ts_game).
# The release tarball is downloaded into the build tree and checked by hash;
# nothing from it is committed here.
#
# Built as C++ on purpose: lua_error() then unwinds with exceptions instead of
# longjmp, so host-side destructors always run. Only the core VM plus the
# pure libraries are compiled; the OS / io / package / debug libraries and the
# stand-alone interpreters are left out entirely, so scripts cannot reach the
# file system or the OS even if the sandbox had a gap.

include(FetchContent)

FetchContent_Declare(
  ts_lua
  URL      https://www.lua.org/ftp/lua-5.4.8.tar.gz
  URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# The tarball ships a Makefile, not a CMakeLists.txt, so this only populates
# the source dir; the library target is declared below.
FetchContent_MakeAvailable(ts_lua)

set(TS_LUA_DIR ${ts_lua_SOURCE_DIR}/src)
set(TS_LUA_SOURCES
  lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldebug.c ldo.c
  ldump.c lfunc.c lgc.c llex.c lmathlib.c lmem.c lobject.c lopcodes.c
  lparser.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c
  lutf8lib.c lvm.c lzio.c
)
# Left out: lua.c luac.c (programs), linit.c (opens every library),
# loslib.c liolib.c loadlib.c ldblib.c (OS, files, modules, debug), and
# lstate.c, which is compiled through LuaFixedSeed.cpp with a fixed hash seed
# so table iteration order is the same on every machine and every run.
list(TRANSFORM TS_LUA_SOURCES PREPEND ${TS_LUA_DIR}/)
set_source_files_properties(${TS_LUA_SOURCES} PROPERTIES LANGUAGE CXX)

add_library(ts_lua STATIC
  ${TS_LUA_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/lua/LuaFixedSeed.cpp
)
target_include_directories(ts_lua PUBLIC ${TS_LUA_DIR})
target_compile_features(ts_lua PUBLIC cxx_std_20)
# Third-party code: keep our own build warning-clean without patching it.
if(MSVC)
  target_compile_options(ts_lua PRIVATE /w)
  target_compile_definitions(ts_lua PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(ts_lua PRIVATE -w)
endif()

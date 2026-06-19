# Nostalgia2P - a TitanAPI sample: a 2-player multiplayer (Last One Standing) mission, adapted from the classic
# C++ SDK "Nostalgia" mission. Two mirrored bases at opposite corners of opu_02.map, a full mining-beacon field,
# and stocked Structure Factories - a clean multiplayer test that showcases the BaseBuilder's offset reuse
# (the same relative layout stamped at two corners) and the multiplayer mission contract.
#
# Like the other samples, this #includes the LIVE library from ../../TitanAPI/include.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cNostalgia2P.dll  (drop into your OPU install; logs to OPU\logs\cNostalgia2P.log)

cmake_minimum_required(VERSION 3.21)
project(Nostalgia2P VERSION 1.0.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: 2P multiplayer (Nostalgia)")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

set(RC_OUTPUT_NAME "cNostalgia2P")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cNostalgia2P SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
  op2_crash.hpp     # listed for IDEs
)

target_include_directories(cNostalgia2P PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cNostalgia2P PRIVATE
  TITANAPI_VERSION="${PROJECT_VERSION}"
  OP2LOG_NAME="cNostalgia2P")

target_compile_options(cNostalgia2P PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cNostalgia2P PROPERTIES OUTPUT_NAME "cNostalgia2P" PREFIX "")
set_property(TARGET cNostalgia2P PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

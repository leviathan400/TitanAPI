# ColdFront - a TitanAPI sample mission: a port of OP2Lua's "Cold Front" reference mission. A single-player
# Eden Colony on on1_01.map, next to a fully-scripted Plymouth AI, with a volcano that erupts and floods the
# lowlands - survive and evacuate to the starship. Showcases the whole Layer 2 facade: the BaseBuilder, the
# trigger/scheduler model, mining + combat orders, ScGroup patrols, the lava disaster, and starship victory.
#
# Like samples/ColonyDemo, this #includes the LIVE library from ../../TitanAPI/include, so it always builds
# against the current op2:: API.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cColdFrontTitan.dll  (drop into your OPU install; logs to OPU\logs\cColdFrontTitan.log)

cmake_minimum_required(VERSION 3.21)
project(ColdFront VERSION 1.0.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: Cold Front")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

# NOTE: output is cColdFrontTitan.dll (NOT cColdFront.dll) - the OP2Lua "Cold Front" sample already builds a
# cColdFront.dll, and two mission DLLs with the same filename in OPU's scanned folders would clash.
set(RC_OUTPUT_NAME "cColdFrontTitan")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cColdFrontTitan SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
  op2_crash.hpp     # listed for IDEs
)

# This sample's scaffolding lives here; the op2:: library is the live TitanAPI include tree.
target_include_directories(cColdFrontTitan PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cColdFrontTitan PRIVATE
  TITANAPI_VERSION="${PROJECT_VERSION}"
  OP2LOG_NAME="cColdFrontTitan")   # main log = cColdFrontTitan.log; AI channel = cColdFrontTitan-AI.log
target_compile_options(cColdFrontTitan PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cColdFrontTitan PROPERTIES OUTPUT_NAME "cColdFrontTitan" PREFIX "")
set_property(TARGET cColdFrontTitan PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Starship Victory - a TitanAPI demo of the Eden/Plymouth "space race" victory system: a single colony with a
# Spaceport whose mission objectives are to build + launch every starship component. Builds to cStarship.dll.
# Like every sample it #includes the LIVE library from ../../TitanAPI/include.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cStarship.dll  (drop into your Outpost 2 / OPU install; logs to OPU\logs\cStarship.log)
# Uses the stock cm01.map + MULTITEK.TXT (present in a standard install).

cmake_minimum_required(VERSION 3.21)
project(Starship VERSION 0.1.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: Starship Victory")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

set(RC_OUTPUT_NAME "cStarship")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cStarship SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
  op2_crash.hpp     # listed for IDEs
)

target_include_directories(cStarship PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cStarship PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}" OP2LOG_NAME="cStarship")
target_compile_options(cStarship PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cStarship PROPERTIES OUTPUT_NAME "cStarship" PREFIX "")
set_property(TARGET cStarship PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Mining - the simplest TitanAPI resource-pipeline sample: a tiny colony (Command Center, Tokamak, Agridome,
# Common Ore Smelter) that surveys a beacon with a Robo-Surveyor, deploys a mine onto it with a Robo-Miner, and
# puts two Cargo Trucks on a haul route. No AI groups, no victory - just "find ore -> build mine -> haul".
# Builds to cMining.dll. Like every sample it #includes the LIVE library from ../../TitanAPI/include.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cMining.dll  (drop into your Outpost 2 / OPU install; logs to OPU\logs\cMining.log)
# Uses the stock cm01.map + MULTITEK.TXT (present in a standard install).

cmake_minimum_required(VERSION 3.21)
project(Mining VERSION 0.1.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: Mining (survey -> deploy -> haul)")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

set(RC_OUTPUT_NAME "cMining")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cMining SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
  op2_crash.hpp     # listed for IDEs
)

target_include_directories(cMining PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cMining PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}" OP2LOG_NAME="cMining")
target_compile_options(cMining PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cMining PROPERTIES OUTPUT_NAME "cMining" PREFIX "")
set_property(TARGET cMining PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

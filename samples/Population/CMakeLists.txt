# Population Goal - a TitanAPI demo of a custom victory condition: grow the colony to a target population.
# Builds to cPopulation.dll. Like every sample it #includes the LIVE library from ../../TitanAPI/include.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cPopulation.dll  (drop into your Outpost 2 / OPU install; logs to OPU\logs\cPopulation.log)
# Uses the stock cm01.map + MULTITEK.TXT (present in a standard install).

cmake_minimum_required(VERSION 3.21)
project(Population VERSION 0.1.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: Population Goal")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

set(RC_OUTPUT_NAME "cPopulation")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cPopulation SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  ../common/op2_mission.hpp   # shared scaffolding (samples/common) - listed for IDEs
  ../common/op2_log.hpp       # shared scaffolding - listed for IDEs
  ../common/op2_crash.hpp     # shared scaffolding - listed for IDEs
)

target_include_directories(cPopulation PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../common"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cPopulation PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}" OP2LOG_NAME="cPopulation")
target_compile_options(cPopulation PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cPopulation PROPERTIES OUTPUT_NAME "cPopulation" PREFIX "")
set_property(TARGET cPopulation PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Rising From The Ashes - a TitanAPI port of Vagabond's OP2MissionSDK colony/space-race mission.
# Builds to cRisingAshes.dll. Like every sample it #includes the LIVE library from ../../TitanAPI/include.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cRisingAshes.dll  (drop into your Outpost 2 / OPU install; logs to OPU\logs\cRisingAshes.log)
# NEEDS in your install: Ashes.map and multitekNoLavaWalls.txt (both ship in the original mission's repo).

cmake_minimum_required(VERSION 3.21)
project(RisingFromTheAshes VERSION 1.0.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: Rising From The Ashes")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

set(RC_OUTPUT_NAME "cRisingAshes")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cRisingAshes SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  ../common/op2_mission.hpp   # shared scaffolding (samples/common) - listed for IDEs
  ../common/op2_log.hpp       # shared scaffolding - listed for IDEs
  ../common/op2_crash.hpp     # shared scaffolding - listed for IDEs
)

target_include_directories(cRisingAshes PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../common"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cRisingAshes PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}" OP2LOG_NAME="cRisingAshes")
target_compile_options(cRisingAshes PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cRisingAshes PROPERTIES OUTPUT_NAME "cRisingAshes" PREFIX "")
set_property(TARGET cRisingAshes PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

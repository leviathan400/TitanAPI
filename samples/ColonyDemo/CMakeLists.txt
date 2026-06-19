# ColonyDemo - a TitanAPI sample mission showcasing the Layer 2 facade (Modules 1-3): a two-player colony
# with unit orders, building, scripted + organic mining, an EMP-capture, a timed Earthworker, range-based
# enumeration, messages and steady morale. Snapshot of the dev test mission.
#
# Unlike samples/Layer1 (which vendors its own frozen headers), this sample #includes the LIVE library from
# ../../TitanAPI/include, so it always builds against the current op2:: API.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cColonyDemo.dll  (drop into your OPU maps\ folder; logs to OPU\logs\cColonyDemo.log)

cmake_minimum_required(VERSION 3.21)
project(ColonyDemo VERSION 0.3.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI sample: full colony demo")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

# Version-info resource (Details tab). FileDescription = TitanAPI, Copyright = Outpost Universe.
set(RC_OUTPUT_NAME "cColonyDemo")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cColonyDemo SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
)

# This sample's mission scaffolding lives here; the op2:: library is the live TitanAPI include tree.
target_include_directories(cColonyDemo PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cColonyDemo PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}")
target_compile_options(cColonyDemo PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cColonyDemo PROPERTIES OUTPUT_NAME "cColonyDemo" PREFIX "")
set_property(TARGET cColonyDemo PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

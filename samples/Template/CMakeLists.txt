# Template - a clean TitanAPI mission to copy and start from. Builds to cTemplate.dll.
#
# To make your own mission: copy this folder, rename it, and change every "Template"/"cTemplate" below to your
# mission's name. Like every sample, it #includes the LIVE library from ../../TitanAPI/include (the op2:: facade
# is header-only - nothing to link), so it always builds against the current API.
#
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cTemplate.dll  (drop into your Outpost 2 / OPU install; logs to OPU\logs\cTemplate.log)

cmake_minimum_required(VERSION 3.21)
project(Template VERSION 1.0.0 LANGUAGES CXX RC DESCRIPTION "TitanAPI mission template")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Must be a 32-bit (x86) build. Re-configure with -A Win32.")
endif()

# Version-info resource (the Windows "Details" tab). FileDescription = TitanAPI, Copyright = Outpost Universe.
set(RC_OUTPUT_NAME "cTemplate")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cTemplate SHARED
  mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  op2_mission.hpp   # listed for IDEs
  op2_log.hpp       # listed for IDEs
  op2_crash.hpp     # listed for IDEs
)

# This mission's scaffolding lives here; the op2:: library is the live TitanAPI include tree.
target_include_directories(cTemplate PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../TitanAPI/include")

target_compile_definitions(cTemplate PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}")
target_compile_options(cTemplate PRIVATE /std:c++23preview)   # genuine C++23 (not the C++26 /std:c++latest track)

set_target_properties(cTemplate PROPERTIES OUTPUT_NAME "cTemplate" PREFIX "")
set_property(TARGET cTemplate PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

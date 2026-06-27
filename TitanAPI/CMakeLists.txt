# TitanAPI - the test-mission DLL (demonstrates the Layer 2 facade over op2::abi against the live game).
# 32-bit / MSVC / C++23. Produces cTitanAPI.dll (the leading 'c' files it under Colony Games).
#
# Configure & build (Visual Studio generator - note the REQUIRED 32-bit platform):
#     cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
#     cmake --build build --config Release
# Output: build/Release/cTitanAPI.dll

cmake_minimum_required(VERSION 3.21)
project(TitanAPI VERSION 0.6.4 LANGUAGES CXX RC DESCRIPTION "Outpost 2 TitanAPI test mission DLL")

if(NOT MSVC)
  message(FATAL_ERROR "Outpost 2 is a 32-bit MSVC target - build with MSVC (cl.exe).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR
    "Must be a 32-bit (x86) build. Re-configure with:  cmake -S . -B build -G \"Visual Studio 18 2026\" -A Win32")
endif()

# Version-info resource (fills the Windows "Details" tab: File description = TitanAPI, Copyright = Outpost
# Universe). Generated from version.rc.in so the version tracks PROJECT_VERSION.
set(RC_OUTPUT_NAME "cTitanAPI")
configure_file(version.rc.in "${CMAKE_CURRENT_BINARY_DIR}/version.rc" @ONLY)

add_library(cTitanAPI SHARED
  src/mission.cpp
  "${CMAKE_CURRENT_BINARY_DIR}/version.rc"
  src/op2_mission.hpp                 # listed for IDEs
  src/op2_log.hpp                     # listed for IDEs
  include/op2/abi/memory.hpp          # listed for IDEs
  include/op2/abi/enums.hpp           # listed for IDEs
)

target_include_directories(cTitanAPI PRIVATE include src)

# Make the project version available to the code (used in the startup log line).
target_compile_definitions(cTitanAPI PRIVATE TITANAPI_VERSION="${PROJECT_VERSION}")

# --- Target C++23 (genuine), no C++26 ---
# NOTE: CMake's cxx_std_23 / CMAKE_CXX_STANDARD 23 map to MSVC's /std:c++latest, which is the C++26 track.
# MSVC's real "up to C++23" switch is /std:c++23preview, so we set it directly. (Verified: std::expected,
# std::print, std::ranges, and deducing-this all compile under it.)
target_compile_options(cTitanAPI PRIVATE /std:c++23preview)

# Output exactly "cTitanAPI.dll" (no lib prefix).
set_target_properties(cTitanAPI PROPERTIES OUTPUT_NAME "cTitanAPI" PREFIX "")

# Static CRT so the DLL has no VC-runtime redistributable dependency (matches the other OP2 mission DLLs),
# which makes it load cleanly into any Outpost 2 install. Requires CMake policy CMP0091 (>= 3.21, NEW).
set_property(TARGET cTitanAPI PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

cmake_minimum_required(VERSION 3.13)

# -----------------------------------------------------------------------------
# Project Settings
# -----------------------------------------------------------------------------
project(Quake2RereleaseClang
    VERSION 1.0
    DESCRIPTION "Quake 2 Re-release Game Module for Windows (Clang Cross-Compilation)"
    LANGUAGES C CXX
)

# Enable IDE folder organization and export compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CROSS_COMPILE_WIN "Cross-compile for Windows using Clang" ON)
option(BUILD_STATIC_LIBRARY "Build as a static library instead of a shared one" OFF)
option(ENABLE_LTO "Enable Link Time Optimization" OFF)
option(STEAM_ASSETS_PATH "Path to Steam Quake 2 assets" "/mnt/storage/SteamLibrary/steamapps/common/Quake 2")

# -----------------------------------------------------------------------------
# Compiler and Language Settings
# -----------------------------------------------------------------------------
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/*.c"
)

# -----------------------------------------------------------------------------
# Library Target
# -----------------------------------------------------------------------------
if(BUILD_STATIC_LIBRARY)
    add_library(game STATIC ${SOURCES})
else()
    add_library(game SHARED ${SOURCES})
endif()

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
target_include_directories(game
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/rerelease"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/bots"
        "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/ctf"
        "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/rogue"
        "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/xatrix"
)

# -----------------------------------------------------------------------------
# Preprocessor Definitions
# -----------------------------------------------------------------------------
target_compile_definitions(game
    PRIVATE
        KEX_Q2_GAME
        KEX_Q2GAME_EXPORTS
        KEX_Q2GAME_DYNAMIC
        _CRT_SECURE_NO_WARNINGS
        _CONSOLE
        STEAM_ASSETS_PATH="${STEAM_ASSETS_PATH}"
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
)

# -----------------------------------------------------------------------------
# External Dependencies (fmt and jsoncpp)
# -----------------------------------------------------------------------------

# --- fmt (Header-Only) ---
find_package(fmt CONFIG REQUIRED)
target_link_libraries(game PRIVATE fmt::fmt-header-only)

# --- jsoncpp (Conditional Logic) ---
if(CROSS_COMPILE_WIN)
    # Find JsonCpp *within the MinGW-w64 sysroot*.
    find_path(JSONCPP_INCLUDE_DIR
        NAMES json/json.h
        PATHS "/usr/x86_64-w64-mingw32/include"
        NO_DEFAULT_PATH
        PATH_SUFFIXES jsoncpp
    )
    find_library(JSONCPP_LIBRARY
        NAMES jsoncpp
        PATHS "/usr/x86_64-w64-mingw32/lib"
        NO_DEFAULT_PATH
    )

    if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
        message(FATAL_ERROR "JsonCpp not found within MinGW-w64 sysroot.")
    endif()

    target_include_directories(game PRIVATE ${JSONCPP_INCLUDE_DIR})
    target_link_libraries(game PRIVATE ${JSONCPP_LIBRARY})
else()
    # Standard Linux JsonCpp setup (using pkg-config)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    target_include_directories(game PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_link_libraries(game PRIVATE ${JSONCPP_LIBRARIES})
endif()


# -----------------------------------------------------------------------------
#  Cross-Compilation Settings
# -----------------------------------------------------------------------------
if(CROSS_COMPILE_WIN)
    message(STATUS "Configuring Windows cross-compilation with Clang")

    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_SYSTEM_PROCESSOR x86_64)

    # --- Use CLANG ---
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)

    # Set the sysroot.
    set(CMAKE_SYSROOT /usr/x86_64-w64-mingw32)
    set(triple x86_64-w64-mingw32)

    # --- Find GCC Library Path (Dynamically) ---
    find_path(MINGW_GCC_LIB_PATH
        NAMES libgcc.a libgcc_s.a libstdc++.a
        PATHS "/usr/lib/gcc/${triple}/10-win32"
        NO_DEFAULT_PATH
        DOC "Path to MinGW-w64 GCC libraries"
    )

    if(NOT MINGW_GCC_LIB_PATH)
        message(FATAL_ERROR "Could not find MinGW-w64 GCC libraries (libgcc.a, libstdc++.a).")
    endif()

    # Find MinGW C++ include path - CORRECTED
    find_path(MINGW_INCLUDE_PATH
        NAMES c++/iostream
        PATHS "/usr/lib/gcc/${triple}/10-win32/include"  # Corrected!
        NO_DEFAULT_PATH
        DOC "Path to MinGW-w64 C++ include"
    )
    if(NOT MINGW_INCLUDE_PATH)
        message(FATAL_ERROR "Could not find MinGW-w64 C++ include directory.")
    endif()


    # Set compiler flags.
    set(CMAKE_C_FLAGS "--target=${triple} -fms-extensions" CACHE STRING "C Flags" FORCE)
	set(CMAKE_CXX_FLAGS "--target=${triple} -stdlib=libstdc++ -fms-extensions -I${MINGW_INCLUDE_PATH}/c++ -I${MINGW_INCLUDE_PATH}/c++/${triple}" CACHE STRING "CXX Flags" FORCE)

    # Set up linker flags.
    set(CMAKE_EXE_LINKER_FLAGS "-L${MINGW_GCC_LIB_PATH} -Wl,--subsystem,windows" CACHE STRING "EXE Linker Flags" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS "-L${MINGW_GCC_LIB_PATH} -Wl,--subsystem,windows" CACHE STRING "SHARED Linker Flags" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "MODULE Linker Flags" FORCE)

    # Adjust FIND_XXX() behavior.
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


    # Define Windows-specific macros.
    target_compile_definitions(game PRIVATE _WIN32 WIN32 _WINDOWS)
    target_link_options(game PRIVATE -static-libgcc -static-libstdc++)

    set_target_properties(game PROPERTIES
        OUTPUT_NAME "game_x64"
        PREFIX ""
        SUFFIX ".dll"
    )
else()
    set_target_properties(game PROPERTIES PREFIX "lib" SUFFIX ".so")
endif()

# ... (Rest of the CMakeLists.txt - Warning Config, IPO/LTO, etc. - No Changes Needed) ...
# -----------------------------------------------------------------------------
# Warning Configuration
# -----------------------------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*")
  set(WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Werror=format-security
    -Wfloat-equal
    -Wconversion
    -Wlogical-op
    -Wnull-dereference
    -Woverloaded-virtual
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-class-memaccess
    -Wno-deprecated-copy
    -fms-extensions
    )
  if(WARNINGS_AS_ERRORS)
    list(APPEND WARNING_FLAGS -Werror)
  endif()
elseif(MSVC)
    set(WARNING_FLAGS /W4 /permissive- /volatile:iso /Zc:preprocessor /EHsc)
    if(WARNINGS_AS_ERRORS)
        list(APPEND WARNING_FLAGS /WX)
    endif()
else()
    set(WARNING_FLAGS -Wall -Wextra -Woverloaded-virtual -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -Wno-class-memaccess -Wno-deprecated-copy)
    if(WARNINGS_AS_ERRORS)
      list(APPEND WARNING_FLAGS -Werror)
    endif()
endif()
target_compile_options(game PRIVATE ${WARNING_FLAGS})

# -----------------------------------------------------------------------------
# Enable IPO/LTO if supported and requested
# -----------------------------------------------------------------------------
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    if(IPO_SUPPORTED)
        message(STATUS "IPO/LTO enabled")
        set_target_properties(game PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO not supported: ${IPO_ERROR}")
    endif()
endif()

# -----------------------------------------------------------------------------
# Installation Configuration
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS game
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

if(BUILD_STATIC_LIBRARY)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/rerelease/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/quake2
        FILES_MATCHING PATTERN "*.h"
    )
endif()

# -----------------------------------------------------------------------------
# Target Properties
# -----------------------------------------------------------------------------
set_target_properties(game PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)

# -----------------------------------------------------------------------------
# Configuration Summary
# -----------------------------------------------------------------------------
message(STATUS "Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
if(CROSS_COMPILE_WIN)
    message(STATUS "  Target: Windows (Clang)")
    message(STATUS "  Output: game_x64.dll")
else()
    message(STATUS "  Target: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  Output: libgame.so")
endif()
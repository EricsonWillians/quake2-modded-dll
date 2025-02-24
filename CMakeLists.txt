cmake_minimum_required(VERSION 3.10)

# -----------------------------------------------------------------------------
# Project Settings
# -----------------------------------------------------------------------------
project(Quake2Rerelease
    VERSION 1.0
    DESCRIPTION "Quake 2 Re-release Game Module"
    LANGUAGES CXX
)

# Enable IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Export compile commands (useful for IDEs and code analyzers)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Always generate PIC code for shared libraries (Good practice)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CROSS_COMPILE_WIN "Cross-compile for Windows (MinGW)" OFF)
option(BUILD_STATIC_LIBRARY "Build as a static library instead of a shared one" OFF)
option(ENABLE_LTO "Enable Link Time Optimization" ON)
option(STEAM_ASSETS_PATH "Path to Steam Quake 2 assets" "/mnt/storage/SteamLibrary/steamapps/common/Quake 2")

# -----------------------------------------------------------------------------
# Compiler and Language Settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# -----------------------------------------------------------------------------
# Source Files  (Do this BEFORE defining the target)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/rerelease/*.cpp"
)

# -----------------------------------------------------------------------------
# Library Target (Define the target EARLY)
# -----------------------------------------------------------------------------
if(BUILD_STATIC_LIBRARY)
    add_library(game STATIC ${SOURCES})
else()
    add_library(game SHARED ${SOURCES})
endif()

# Disable Unity build due to header conflicts (NOW we can modify the target)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    set_target_properties(game PROPERTIES
        UNITY_BUILD OFF  # Disable unity build to avoid header conflicts
    )
endif()
# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
target_include_directories(game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/rerelease
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/rerelease/bots
        ${CMAKE_CURRENT_SOURCE_DIR}/rerelease/ctf
        ${CMAKE_CURRENT_SOURCE_DIR}/rerelease/rogue
        ${CMAKE_CURRENT_SOURCE_DIR}/rerelease/xatrix
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
# External Dependencies
# -----------------------------------------------------------------------------

# fmt Library Configuration - Always use header-only for maximum compatibility
find_package(fmt CONFIG REQUIRED)
target_link_libraries(game PRIVATE fmt::fmt-header-only)

# JsonCpp Configuration
if(CROSS_COMPILE_WIN)
    # Find JsonCpp include directory
    find_path(JSONCPP_INCLUDE_DIR
        NAMES json/json.h  # Look for json.h inside a json directory
        PATHS "/usr/x86_64-w64-mingw32/include"  # Use the FULL, exact path
        NO_DEFAULT_PATH  # Only look in the specified path
        NO_CMAKE_ENVIRONMENT_PATH 
        NO_CMAKE_PATH 
        NO_SYSTEM_ENVIRONMENT_PATH 
        NO_CMAKE_SYSTEM_PATH 
        PATH_SUFFIXES jsoncpp
    )

    # Find JsonCpp library
    find_library(JSONCPP_LIBRARY
        NAMES jsoncpp
        PATHS "/usr/x86_64-w64-mingw32/lib"  # Use the FULL, exact path
        NO_DEFAULT_PATH
        NO_CMAKE_ENVIRONMENT_PATH 
        NO_CMAKE_PATH  
        NO_SYSTEM_ENVIRONMENT_PATH  
        NO_CMAKE_SYSTEM_PATH  
    )

    # Check if both were found
    if(NOT JSONCPP_INCLUDE_DIR OR NOT JSONCPP_LIBRARY)
        message(STATUS "JsonCpp headers or library not found in standard MinGW path.")
        message(STATUS "Include Dir: ${JSONCPP_INCLUDE_DIR}") # Debug output
        message(STATUS "Library    : ${JSONCPP_LIBRARY}")   # Debug output
        message(STATUS "You may need to install MinGW JsonCpp (see previous instructions).")
        message(FATAL_ERROR "JsonCpp not found. Exiting.")
    endif()

    target_include_directories(game PRIVATE ${JSONCPP_INCLUDE_DIR})
    target_link_libraries(game PRIVATE ${JSONCPP_LIBRARY})
else()
    # Unix/Linux JsonCpp configuration
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    target_include_directories(game PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_link_libraries(game PRIVATE ${JSONCPP_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Cross-Compilation (MinGW) Setup  (Place AFTER target definition)
# -----------------------------------------------------------------------------
if(CROSS_COMPILE_WIN)
    message(STATUS "Configuring Windows cross-compilation with MinGW")

    # Windows system settings
    set(CMAKE_SYSTEM_NAME Windows)
    set(WINDOWS_TOOLCHAIN_PREFIX "x86_64-w64-mingw32")
    set(CMAKE_C_COMPILER "${WINDOWS_TOOLCHAIN_PREFIX}-gcc")
    set(CMAKE_CXX_COMPILER "${WINDOWS_TOOLCHAIN_PREFIX}-g++")
    set(CMAKE_RC_COMPILER "${WINDOWS_TOOLCHAIN_PREFIX}-windres")
    set(CMAKE_FIND_ROOT_PATH "/usr/${WINDOWS_TOOLCHAIN_PREFIX}")

    # Compiler/linker flags specific to MinGW
    set(MINGW_COMPILE_FLAGS
        "-fexceptions"
        "-fno-omit-frame-pointer"
        "-mwindows"
        "-mconsole"  #Consider removing -mconsole if you don't need a console window
    )
    string(REPLACE ";" " " MINGW_COMPILE_FLAGS "${MINGW_COMPILE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MINGW_COMPILE_FLAGS}")

    # MinGW definitions (These are target specific, so use target_compile_definitions)
    target_compile_definitions(game PRIVATE -D_WIN32 -DWIN32 -D_WINDOWS)
    target_compile_definitions(game PRIVATE FMT_EXCEPTIONS=1)  #Keep definitions together.

     # Set output names based on platform (AFTER target definition)
    set_target_properties(game PROPERTIES
        OUTPUT_NAME "game_x64"
        PREFIX ""
        SUFFIX ".dll"
    )
    target_link_options(game PRIVATE
        -static-libgcc
        -static-libstdc++
    )
else()
    # Set output names based on platform (AFTER target definition)
     set_target_properties(game PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# -----------------------------------------------------------------------------
# Warning Configuration (Keep this after Cross-compile, since flags can be platform specific.)
# -----------------------------------------------------------------------------
if(MSVC)
    set(WARNING_FLAGS /W4 /permissive- /volatile:iso /Zc:preprocessor /EHsc)
    if(WARNINGS_AS_ERRORS)
        list(APPEND WARNING_FLAGS /WX)
    endif()
else()
    set(WARNING_FLAGS
        -Wall
        -Wextra
        -Woverloaded-virtual
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-missing-field-initializers
        -Wno-class-memaccess
        -Wno-deprecated-copy
    )
    if(WARNINGS_AS_ERRORS)
        list(APPEND WARNING_FLAGS -Werror)
    endif()
endif()

# Add warning flags (Use target_compile_options for better control)
target_compile_options(game PRIVATE ${WARNING_FLAGS})

# -----------------------------------------------------------------------------
# Enable IPO/LTO if supported and requested (This is fine where it is)
# -----------------------------------------------------------------------------
if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
    if(IPO_SUPPORTED)
        message(STATUS "IPO/LTO enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(WARNING "IPO/LTO not supported: ${IPO_ERROR}")
    endif()
endif()

# -----------------------------------------------------------------------------
# Platform-specific link options (Move to where the target is configured for the platform)
# -----------------------------------------------------------------------------
# MOVED to Cross-compile section

# -----------------------------------------------------------------------------
# Installation Configuration
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS game
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Optional: Install headers if this becomes a redistributable library
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
    POSITION_INDEPENDENT_CODE ON # Redundant, we set this globally earlier
)

# Enable Unity/Batch building if supported by the CMake version (Moved AFTER target creation)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    set_target_properties(game PROPERTIES
         #UNITY_BUILD ON  <-- DISABLED earlier
        UNITY_BUILD_MODE BATCH
        UNITY_BUILD_BATCH_SIZE 10
    )
endif()
# -----------------------------------------------------------------------------
# Print Configuration Summary
# -----------------------------------------------------------------------------
message(STATUS "Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Unity Build: ${UNITY_BUILD}")  # Will be empty if disabled
message(STATUS "  LTO Enabled: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
if(CROSS_COMPILE_WIN)
    message(STATUS "  Target: Windows (MinGW)")
    message(STATUS "  Output: game_x64.dll")
else()
    message(STATUS "  Target: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "  Output: libgame.so")
endif()
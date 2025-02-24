#!/bin/bash
################################################################################
# compile-windows-clang.sh - Cross-compilation Script for Quake2Rerelease
#                             Windows Build using Clang
#
# This script configures and cross-compiles the Quake2Rerelease project for
# Windows using Clang as the cross-compiler targeting Windows (x86_64).  It
# integrates cross-compilation settings directly into CMakeLists.txt.
#
# Prerequisites:
#   - Clang with MinGW target support (clang, clang++)
#   - MinGW-w64 toolchain installed (libraries and headers)
#   - CMake 3.13+
#   - ninja (sudo apt install ninja-build)
#
# Usage:
#   ./compile-windows-clang.sh
################################################################################

set -euo pipefail

# --- Function Definitions ---

# Prints a formatted header.
print_header() {
  local text="$1"
  local line_length=60
  local padding=$(( (line_length - ${#text} - 2) / 2 ))  # Calculate padding
  printf "%s\n" "$(printf '=%.0s' $(seq 1 $line_length))"
  printf "%*s %s %*s\n" "$padding" "" "$text" "$padding" ""
  printf "%s\n" "$(printf '=%.0s' $(seq 1 $line_length))"
}

# Checks if a command exists.
command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# Checks for required tools and exits if any are missing.
check_prerequisites() {
  local required_tools=("clang++" "cmake" "ninja")
  local missing_tools=()

  for tool in "${required_tools[@]}"; do
    if ! command_exists "$tool"; then
      missing_tools+=("$tool")
    fi
  done

  if [[ ${#missing_tools[@]} -gt 0 ]]; then
    echo "Error: The following required tools are missing:"
    for tool in "${missing_tools[@]}"; do
      echo "  - $tool"
    done
    echo "Please install them before running this script."
    exit 1
  fi
}

# Creates or cleans the build directory.
setup_build_directory() {
  local build_dir="$1"

  if [[ ! -d "$build_dir" ]]; then
    echo "Creating build directory: '$build_dir'"
    mkdir -p "$build_dir" || { echo "Failed to create build directory."; exit 1; }
  else
    echo "Cleaning build directory: '$build_dir'"
    rm -rf "$build_dir"/*  # Clean the directory contents
  fi
}

# Configures CMake for cross-compilation.
configure_cmake() {
    local build_dir="$1"
    local num_cores="$2" # Removed toolchain_file

    echo "Configuring project for Windows (Clang cross-compilation)..."

    cmake -S . -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCROSS_COMPILE_WIN=ON \
        -DENABLE_LTO=OFF \
        -G "Ninja" \
        -DCMAKE_MAKE_PROGRAM="ninja" || { echo "CMake configuration failed."; exit 1; }
        # Removed -DCMAKE_TOOLCHAIN_FILE
}

# Builds the project using CMake.
build_project() {
    local build_dir="$1"
    local num_cores="$2"

    echo "Building Windows DLL with Clang..."
    cmake --build "$build_dir" --parallel "$num_cores" || { echo "Build failed."; exit 1; }
}

# Checks if the build was successful.
check_build_success() {
    local build_dir="$1"
    local expected_output="game_x64.dll"

    if [[ -f "$build_dir/$expected_output" ]]; then
        echo "Build successful! Your DLL is ready."
        echo "You can find it in the $build_dir directory."
    else:
        echo "Build seems to have failed. Check the errors above."
        exit 1
    fi
}

# --- Main Script ---

print_header "Quake2Rerelease Windows Cross-Compilation (Clang)"
check_prerequisites
NUM_CORES=$(nproc 2>/dev/null || echo 1)  # Get number of cores, default to 1
echo "Detected $NUM_CORES available core(s) for compilation."

BUILD_DIR="build-windows-clang"
# TOOLCHAIN_FILE="toolchain-windows-clang.cmake"  <- No longer needed

setup_build_directory "$BUILD_DIR"

configure_cmake "$BUILD_DIR" "$NUM_CORES" # Removed toolchain_file argument
build_project "$BUILD_DIR" "$NUM_CORES"
check_build_success "$BUILD_DIR"

print_header "Windows Build Complete"

exit 0
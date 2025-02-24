#!/bin/bash
################################################################################
# compile-windows-debug.sh - Cross-compilation Debug Script for Quake2Rerelease
#                          Windows Build
#
# This script configures and cross-compiles the Quake2Rerelease project for 
# Windows using MinGW-w64 in Debug mode (with debug symbols).
#
# Prerequisites:
#   - MinGW-w64 toolchain
#   - CMake 3.10+
#
# Usage:
#   ./compile-windows-debug.sh
################################################################################

set -euo pipefail

print_header() {
    echo "==============================================="
    echo "  Quake2Rerelease Windows Debug Cross-Compilation"
    echo "==============================================="
}

print_header

# Check for MinGW-w64 compiler
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Error: MinGW-w64 compiler not found!"
    echo "Please install it with: sudo apt install mingw-w64"
    exit 1
fi

# Determine available cores
NUM_CORES=$(nproc)
echo "Detected $NUM_CORES available core(s) for compilation."

# Define build directory for Windows Debug build
BUILD_DIR="build-windows-debug"

# Create or clean build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating Windows Debug build directory: '$BUILD_DIR'"
    mkdir "$BUILD_DIR"
else
    echo "Windows Debug build directory '$BUILD_DIR' already exists."
fi

# Change to build directory
cd "$BUILD_DIR"

# Configure for Windows cross-compilation in Debug mode
echo "Configuring project for Windows (Debug)..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCROSS_COMPILE_WIN=ON \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY

# Build the project using all available cores
echo "Building Windows Debug DLL..."
cmake --build . --parallel "$NUM_CORES"

echo "==============================================="
echo "         Windows Debug Build Complete         "
echo "==============================================="

# Check for expected output (DLL)
if [ -f "libgame.dll" ] || [ -f "game_x64.dll" ]; then
    echo "Debug build successful! Your debug DLL is ready."
    echo "Find it in the '$BUILD_DIR' directory."
else
    echo "Debug build appears to have failed. Check the errors above."
    exit 1
fi

#!/bin/bash
################################################################################
# compile-windows.sh - Cross-compilation Script for Quake2Rerelease Windows Build
#
# This script configures and cross-compiles the Quake2Rerelease project for 
# Windows using MinGW-w64. It configures CMake appropriately and builds the 
# project in Release mode.
#
# Prerequisites:
#   - MinGW-w64 toolchain
#   - CMake 3.10+
#   - Windows dependencies installed via MinGW
#
# Usage:
#   ./compile-windows.sh
################################################################################

set -euo pipefail

print_header() {
    echo "==============================================="
    echo "  Quake2Rerelease Windows Cross-Compilation    "
    echo "==============================================="
}

print_header

# Check for MinGW compiler
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "Error: MinGW-w64 compiler not found!"
    echo "Please install it with: sudo apt install mingw-w64"
    exit 1
fi

# Determine available cores
NUM_CORES=$(nproc)
echo "Detected $NUM_CORES available core(s) for compilation."

# Define build directory for Windows
BUILD_DIR="build-windows"

# Create or clean build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating Windows build directory: '$BUILD_DIR'"
    mkdir "$BUILD_DIR"
else
    echo "Windows build directory '$BUILD_DIR' already exists."
fi

# Change to build directory
cd "$BUILD_DIR"

# Configure for Windows cross-compilation
echo "Configuring project for Windows..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCROSS_COMPILE_WIN=ON \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY

# Build the project
echo "Building Windows DLL..."
cmake --build . --parallel "$NUM_CORES"

echo "==============================================="
echo "          Windows Build Complete               "
echo "==============================================="

# The output DLL will be named game_x64.dll in the build directory
if [ -f "libgame.dll" ] || [ -f "game_x64.dll" ]; then
    echo "Build successful! Your DLL is ready."
    echo "You can find it in the $BUILD_DIR directory."
else
    echo "Build seems to have failed. Check the errors above."
    exit 1
fi
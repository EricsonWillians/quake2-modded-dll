#!/bin/bash
################################################################################
# compile.sh - Automated Build Script for Quake2Rerelease Project
#
# This script configures and compiles the Quake2Rerelease project using CMake.
# It automatically detects the number of available processor cores and builds
# the project in Release mode in parallel to maximize compilation speed.
#
# Prerequisites:
#   - CMake (version 3.10 or newer)
#   - A C++ compiler with C++17 support
#
# Usage:
#   ./compile.sh
#
# Author: [Your Name]
# Date: [Insert Date]
# License: [Your License Information, e.g., MIT]
################################################################################

# Exit immediately on error, treat unset variables as errors, and
# ensure that any error in a pipeline causes the script to fail.
set -euo pipefail

# Function: print_header
# Prints a header banner for clarity in the console output.
print_header() {
    echo "==============================================="
    echo "       Quake2Rerelease Project Build Script    "
    echo "==============================================="
}

print_header

# Determine the number of available processor cores.
NUM_CORES=$(nproc)
echo "Detected $NUM_CORES available core(s) for compilation."

# Define the build directory.
BUILD_DIR="build"

# Create the build directory if it does not already exist.
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory: '$BUILD_DIR'"
    mkdir "$BUILD_DIR"
else
    echo "Build directory '$BUILD_DIR' already exists."
fi

# Change into the build directory.
cd "$BUILD_DIR"

# Configure the project using CMake in Release mode.
echo "Configuring the project..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project using all available cores.
echo "Building the project in parallel..."
cmake --build . --parallel "$NUM_CORES"

echo "==============================================="
echo "                Build Complete                "
echo "==============================================="

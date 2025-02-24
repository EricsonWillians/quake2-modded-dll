# Build Instructions for Quake2Rerelease (Windows DLL) - Clang Cross-Compilation

This document provides detailed instructions for cross-compiling the Quake2Rerelease project to a Windows DLL (`game_x64.dll`) from a Linux environment using **Clang** and the **MinGW-w64** toolchain.  It specifically addresses the challenges of dependencies (fmt and jsoncpp) and explains why using Clang is *essential* for a working DLL.

**CRITICAL: Why Clang and NOT MinGW GCC for a Functional DLL:**

The standard MinGW GCC compiler (e.g., `x86_64-w64-mingw32-g++`) produces a DLL that, while it compiles without errors, is **non-functional** with the latest Bethesda/Steam Quake 2 executable. This is due to subtle incompatibilities in how MinGW GCC and the official Quake 2 executable handle certain C++ features (likely related to name mangling, calling conventions, or runtime library differences).  

**Only Clang, targeting the MinGW-w64 runtime, has been proven to produce a working `game_x64.dll` that is compatible with the current Quake2.exe.** This build process uses Clang as the cross-compiler, but leverages the MinGW-w64 libraries and headers for Windows API compatibility.

## Prerequisites

Before you begin, ensure you have the following installed on your Linux system (Debian/Ubuntu/Pop!_OS based distributions are assumed in these instructions; adapt as needed for other distributions):

*   **CMake:**  Version 3.13 or later is required.
*   **Clang:**  Clang compiler with support for the MinGW target.
*   **lld:** Clang's linker.
*   **Ninja:** A fast build system (recommended for speed).
*   **MinGW-w64:** The *complete* 64-bit Windows cross-compilation toolchain.  This is *crucial*.
*   **fmt library (header-only):**  We'll use the header-only version for simplicity.
*   **jsoncpp library (MinGW-w64 version):** This will be installed as a dependency.

**Installation Steps (Debian/Ubuntu/Pop!_OS):**

1.  **Install Build Tools:**

    ```bash
    sudo apt update
    sudo apt install build-essential cmake clang lld ninja-build
    ```

2.  **Install MinGW-w64 (Complete Toolchain):**

    It's *essential* to install the full MinGW-w64 cross-compilation environment.  Do *not* rely on partial or potentially incomplete installations.  First, *remove* any existing, potentially conflicting MinGW packages:

    ```bash
    sudo apt purge mingw-w64 mingw-w64-tools mingw-w64-i686-dev mingw-w64-x86-64-dev
    sudo apt autoremove --purge
    ```

    Then, install the complete 64-bit toolchain:

    ```bash
    sudo apt update
    sudo apt install g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-x86-64-dev
    ```

3.  **Install jsoncpp:**

    ```bash
    sudo apt install mingw-w64-x86-64-dev
    ```
    This will install it inside our mingw sysroot.

4.  **Install fmt (Header-Only):**

    *   Download the latest header-only release from the `fmt` GitHub repository: [https://github.com/fmtlib/fmt/releases](https://github.com/fmtlib/fmt/releases) (look for a `.zip` or `.tar.gz` file).
    *   Extract the archive.
    *   Copy the contents of the `include/fmt` directory within the extracted archive to the MinGW-w64 include directory:

        ```bash
        sudo mkdir -p /usr/x86_64-w64-mingw32/include/fmt
        sudo cp -r /path/to/extracted/fmt/include/fmt/* /usr/x86_64-w64-mingw32/include/fmt/
        ```

        Replace `/path/to/extracted/fmt/` with the actual path.

5. **Verify Installation (Important!)**

    After installation, *verify* that the crucial MinGW-w64 libraries and headers are present:

    ```bash
    find /usr/x86_64-w64-mingw32 -name libgcc.a
    find /usr/x86_64-w64-mingw32 -name libgcc_s.a
    find /usr/x86_64-w64-mingw32 -name libstdc++.a
    find /usr/lib/gcc/x86_64-w64-mingw32/ -name iostream -print
    ```

    These commands *must* produce output showing the paths to these files. If any of them return nothing, your MinGW-w64 installation is still incomplete, and you need to revisit step 2.  The `iostream` check should show a path similar to `/usr/lib/gcc/x86_64-w64-mingw32/10-win32/include/c++/iostream`.

## Build Script (`compile-windows-clang.sh`)

```bash
#!/bin/bash
################################################################################
# compile-windows-clang.sh - Cross-compilation Script for Quake2Rerelease
#                             Windows Build using Clang
#
# This script configures and cross-compiles the Quake2Rerelease project for
# Windows using Clang as the cross-compiler targeting Windows (x86_64).
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
  local text="<span class="math-inline">1"
local line\_length\=60
local padding\=</span>(( (line_length - <span class="math-inline">\{\#text\} \- 2\) / 2 \)\)  \# Calculate padding
printf "%s\\n" "</span>(printf '=%.0s' $(seq 1 $line_length))"
  printf "%*s %s %*s\n" "$padding" "" "$text" "<span class="math-inline">padding" ""
printf "%s\\n" "</span>(printf '=%.0s' $(seq 1 $line_length))"
}

# Checks if a command exists.
command_exists() {
  command -v "<span class="math-inline">1" \>/dev/null 2\>&1
\}
\# Checks for required tools and exits if any are missing\.
check\_prerequisites\(\) \{
local required\_tools\=\("clang\+\+" "cmake" "ninja"\)
local missing\_tools\=\(\)
for tool in "</span>{required_tools[@]}"; do
    if ! command_exists "$tool"; then
      missing_tools+=("$tool")
    fi
  done

  if [[ <span class="math-inline">\{\#missing\_tools\[@\]\} \-gt 0 \]\]; then
echo "Error\: The following required tools are missing\:"
for tool in "</span>{missing_tools[@]}"; do
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
    local num_cores="$2"

    echo "Configuring project for Windows (Clang cross-compilation)..."

    cmake -S . -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCROSS_COMPILE_WIN=ON \
        -DENABLE_LTO=OFF \
        -G "Ninja" \
        -DCMAKE_MAKE_PROGRAM="ninja" || { echo "CMake configuration failed."; exit 1; }
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
        echo "You can find it in the <span class="math-inline">build\_dir directory\."
else\:
echo "Build seems to have failed\. Check the errors above\."
exit 1
fi
\}
\# \-\-\- Main Script \-\-\-
print\_header "Quake2Rerelease Windows Cross\-Compilation \(Clang\)"
check\_prerequisites
NUM\_CORES\=</span>(nproc 2>/dev/null || echo 1)  # Get number of cores, default to 1
echo "Detected $NUM_CORES available core(s) for compilation."

BUILD_DIR="build-windows-clang"

setup_build_directory "$BUILD_DIR"

configure_cmake "$BUILD_DIR" "$NUM_CORES"
build_project "$BUILD_DIR" "$NUM_CORES"
check_build_success "$BUILD_DIR"

print_header "Windows Build Complete"

exit 0
```
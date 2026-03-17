#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${1:-build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

echo "Using build directory: ${BUILD_DIR}"
echo "Using build type: ${BUILD_TYPE}"

cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}"

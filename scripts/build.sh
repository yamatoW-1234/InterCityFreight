#!/usr/bin/env bash
# ============================================================================
#  build.sh — Compile the C++ algorithmic engine on macOS.
#  Prefers CMake; falls back to a direct clang++ build if CMake is absent.
# ============================================================================
set -e
cd "$(dirname "$0")/.."

CXX="${CXX:-clang++}"
INC="algorithm_engine/include"

if command -v cmake >/dev/null 2>&1; then
  echo "==> Building with CMake"
  cmake -S algorithm_engine -B algorithm_engine/build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build algorithm_engine/build --parallel
else
  echo "==> CMake not found — building directly with $CXX"
  "$CXX" -std=c++17 -O2 -Wall -Wextra -I "$INC" \
    algorithm_engine/src/*.cpp \
    -o algorithm_engine/freight_engine
fi

if [ -x algorithm_engine/freight_engine ]; then
  echo "==> Build OK: algorithm_engine/freight_engine"
else
  echo "==> Build FAILED: binary not found"; exit 1
fi
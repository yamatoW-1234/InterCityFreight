#!/usr/bin/env bash
# ============================================================================
#  run_tests.sh — Build and run the full test suite on macOS.
#  Usage:  bash tests/run_tests.sh
# ============================================================================
set -e
cd "$(dirname "$0")/.."          # project root

CXX="${CXX:-clang++}"
INC="algorithm_engine/include"

ENGINE_SRCS=(
  algorithm_engine/src/graph.cpp
  algorithm_engine/src/avl_tree.cpp
  algorithm_engine/src/avl_pq.cpp
  algorithm_engine/src/b_tree.cpp
  algorithm_engine/src/dijkstra.cpp
  algorithm_engine/src/astar.cpp
  algorithm_engine/src/bellman_ford.cpp
  algorithm_engine/src/prim.cpp
  algorithm_engine/src/engine.cpp
)

echo "==> [1/3] Building C++ unit-test binary"
mkdir -p algorithm_engine/build_test
"$CXX" -std=c++17 -O2 -Wall -Wextra -I "$INC" \
  "${ENGINE_SRCS[@]}" tests/cpp/test_main.cpp \
  -o algorithm_engine/build_test/freight_tests

echo "==> [2/3] Running C++ unit tests"
./algorithm_engine/build_test/freight_tests

echo "==> [3/3] Building engine binary + running integration tests"
"$CXX" -std=c++17 -O2 -I "$INC" \
  "${ENGINE_SRCS[@]}" algorithm_engine/src/main.cpp \
  -o algorithm_engine/freight_engine
python3 tests/test_engine.py

echo "==> All test stages complete"
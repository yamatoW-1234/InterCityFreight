cmake_minimum_required(VERSION 3.14)
project(FreightEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(ENGINE_SOURCES
    src/graph.cpp
    src/avl_tree.cpp
    src/avl_pq.cpp
    src/b_tree.cpp
    src/dijkstra.cpp
    src/astar.cpp
    src/bellman_ford.cpp
    src/prim.cpp
    src/engine.cpp
    src/main.cpp
)

add_executable(freight_engine ${ENGINE_SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(freight_engine PRIVATE -Wall -Wextra -O2)
endif()

# Place the binary at the algorithm_engine/ root so the Flask backend can find it easily.
set_target_properties(freight_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
// ============================================================================
//  dijkstra.hpp — Dijkstra's shortest-path algorithm (non-negative weights).
//  The priority queue is the AVL-backed indexed min-PQ (avl_pq.hpp).
//  Works in Distance mode or non-negative Financial mode.
// ============================================================================
#pragma once
#include "route_result.hpp"

namespace freight {

RouteResult dijkstra(const Graph& g, int source, int dest, WeightMode mode);

} // namespace freight
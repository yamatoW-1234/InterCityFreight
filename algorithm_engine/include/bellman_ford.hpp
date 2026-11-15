// ============================================================================
//  bellman_ford.hpp — Bellman-Ford shortest path for the financial objective.
//  Edge weight = toll + fuel - subsidy. Supports negative edges and detects
//  reachable negative-weight cycles (reporting them as errors, never as
//  realistic savings).
// ============================================================================
#pragma once
#include "route_result.hpp"

namespace freight {

RouteResult bellman_ford(const Graph& g, int source, int dest);

} // namespace freight
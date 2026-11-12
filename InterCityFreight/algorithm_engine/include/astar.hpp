// ============================================================================
//  astar.hpp — A* search for the fastest freight-transit route.
//  Edge weight = estimated travel time. Heuristic = Haversine straight-line
//  distance to the goal divided by the maximum speed in the dataset, which is
//  an admissible and consistent lower bound on remaining travel time.
// ============================================================================
#pragma once
#include "route_result.hpp"

namespace freight {

RouteResult astar(const Graph& g, int source, int dest);

} // namespace freight
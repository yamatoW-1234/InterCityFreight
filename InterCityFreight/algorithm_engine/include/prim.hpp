// ============================================================================
//  prim.hpp — Prim's Minimum Spanning Tree for multi-hub infrastructure planning.
//
//  Prim's runs on the METRIC CLOSURE of the selected hubs: the weight between
//  two hubs is their shortest-path road distance (computed with Dijkstra).
//  The result is the minimal highway backbone that links every selected hub.
//  This is infrastructure planning, NOT a point-to-point delivery route.
// ============================================================================
#pragma once
#include "route_result.hpp"
#include <vector>

namespace freight {

struct MSTEdgeResult {
    int    from_hub = -1;       // city id of first hub
    int    to_hub   = -1;       // city id of second hub
    double distance_km = 0.0;   // shortest-path corridor distance
    double cost        = 0.0;   // financial cost along the corridor
    std::vector<int>  corridor_path;    // actual city sequence of the corridor
    std::vector<Edge> corridor_edges;
};

struct MSTResult {
    bool        success = false;
    std::string status_message;
    std::vector<int> hubs;                     // selected hub city ids
    std::vector<MSTEdgeResult> mst_edges;
    double total_distance_km = 0.0;
    double total_cost        = 0.0;
    double execution_time_ms = 0.0;
    bool   disconnected      = false;
    std::vector<int> unreachable_hubs;         // city ids not reachable by the MST
};

MSTResult prim_mst(const Graph& g, const std::vector<int>& hubs);

} // namespace freight
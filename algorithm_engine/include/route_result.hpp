// ============================================================================
//  route_result.hpp — Common result object returned by every routing algorithm.
//  Holds the reconstructed path, per-mode metrics, and execution statistics.
// ============================================================================
#pragma once
#include "graph.hpp"
#include <vector>
#include <string>

namespace freight {

struct RouteResult {
    bool        success = false;
    std::string algorithm;              // "A* Search", "Dijkstra", "Bellman-Ford", ...
    std::string status_message;         // error / diagnostic detail when !success

    std::vector<int>  path;             // ordered city ids, source -> destination
    std::vector<Edge> path_edges;       // directed edges used, parallel to `path`

    // Operational metrics (Module 2 / synopsis §3).
    double total_distance_km = 0.0;
    double total_time_hours  = 0.0;
    double total_toll        = 0.0;
    double total_fuel        = 0.0;
    double total_subsidy     = 0.0;
    double total_cost        = 0.0;     // toll + fuel - subsidy

    // Algorithm performance statistics.
    int    expanded_vertices = 0;
    double execution_time_ms = 0.0;

    // Bellman-Ford specific.
    bool   negative_cycle = false;
};

// Sum every operational metric across a sequence of directed edges.
inline void accumulate_metrics(const Graph& g, const std::vector<Edge>& edges, RouteResult& r) {
    r.total_distance_km = r.total_time_hours = 0.0;
    r.total_toll = r.total_fuel = r.total_subsidy = r.total_cost = 0.0;
    for (const Edge& e : edges) {
        r.total_distance_km += e.distance_km;
        r.total_time_hours  += e.time_hours();
        r.total_toll        += e.toll;
        r.total_fuel        += g.fuel_cost(e.distance_km);
        r.total_subsidy     += e.subsidy;
        r.total_cost        += g.financial_cost(e);
    }
}

} // namespace freight
// ============================================================================
//  bellman_ford.cpp — Implementation of the Bellman-Ford algorithm.
// ============================================================================
#include "bellman_ford.hpp"
#include <chrono>
#include <algorithm>
#include <limits>

namespace freight {

RouteResult bellman_ford(const Graph& g, int source, int dest) {
    RouteResult r;
    r.algorithm = "Bellman-Ford";

    const int n = g.num_vertices();
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        r.status_message = "invalid source or destination index";
        return r;
    }

    auto t0 = std::chrono::steady_clock::now();

    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> dist(n, INF);
    std::vector<int>    pred(n, -1);
    std::vector<Edge>   pred_edge(n);
    dist[source] = 0.0;

    const std::vector<Edge>& edges = g.edges();

    // Relax all edges up to V-1 times (early exit when a pass changes nothing).
    long long relaxations = 0;
    for (int i = 0; i < n - 1; ++i) {
        bool changed = false;
        for (const Edge& e : edges) {
            if (dist[e.from] == INF) continue;
            const double nd = dist[e.from] + g.financial_cost(e);
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                pred[e.to] = e.from;
                pred_edge[e.to] = e;
                changed = true;
                ++relaxations;
            }
        }
        if (!changed) break;
    }

    // Detect nodes whose distance is unbounded (reachable negative cycle).
    std::vector<char> affected(n, 0);
    for (const Edge& e : edges) {
        if (dist[e.from] != INF && dist[e.from] + g.financial_cost(e) < dist[e.to])
            affected[e.to] = 1;
    }
    // Forward-propagate the "affected" flag to every node reachable from a cycle.
    for (int i = 0; i < n; ++i)
        for (const Edge& e : edges)
            if (affected[e.from]) affected[e.to] = 1;

    auto t1 = std::chrono::steady_clock::now();
    r.execution_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.expanded_vertices = (int)relaxations;      // number of successful relaxations

    if (affected[dest]) {
        r.success = false;
        r.negative_cycle = true;
        r.status_message = "a reachable negative-weight cycle affects the destination; "
                           "the minimum cost is unbounded and no finite route is returned";
        return r;
    }

    if (dist[dest] == INF) {
        r.success = false;
        r.status_message = "destination is unreachable from source";
        return r;
    }

    // Path reconstruction.
    std::vector<int>  path;
    std::vector<Edge> path_edges;
    int cur = dest;
    while (cur != -1) {
        path.push_back(cur);
        if (cur == source) break;
        path_edges.push_back(pred_edge[cur]);
        cur = pred[cur];
    }
    std::reverse(path.begin(), path.end());
    std::reverse(path_edges.begin(), path_edges.end());

    r.success = true;
    r.path = path;
    r.path_edges = path_edges;
    accumulate_metrics(g, path_edges, r);
    return r;
}

} // namespace freight
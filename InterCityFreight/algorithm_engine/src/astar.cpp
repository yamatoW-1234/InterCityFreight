// ============================================================================
//  astar.cpp — Implementation of A* search with a Haversine time heuristic.
// ============================================================================
#include "astar.hpp"
#include "avl_pq.hpp"
#include "haversine.hpp"
#include <chrono>
#include <algorithm>
#include <limits>

namespace freight {

RouteResult astar(const Graph& g, int source, int dest) {
    RouteResult r;
    r.algorithm = "A* Search";

    const int n = g.num_vertices();
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        r.status_message = "invalid source or destination index";
        return r;
    }

    const City& goal = g.city(dest);
    const double max_speed = g.max_speed();      // >= every edge speed => admissible

    // h(u) = straight-line distance to goal / max speed  (lower bound on time).
    auto heuristic = [&](int u) -> double {
        const City& c = g.city(u);
        return geo::haversine_km(c.lat, c.lon, goal.lat, goal.lon) / max_speed;
    };

    auto t0 = std::chrono::steady_clock::now();

    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> gscore(n, INF);          // best known cost-from-source
    std::vector<int>    pred(n, -1);
    std::vector<Edge>   pred_edge(n);
    std::vector<char>   closed(n, 0);

    AVLPriorityQueue open;                       // keyed by f = g + h
    gscore[source] = 0.0;
    open.push_or_decrease(source, heuristic(source));

    int expanded = 0;
    while (!open.empty()) {
        int u; double f;
        open.pop_min(u, f);
        if (closed[u]) continue;
        closed[u] = 1;
        ++expanded;
        if (u == dest) break;

        for (const Edge& e : g.adj()[u]) {
            const int v = e.to;
            if (closed[v]) continue;
            const double ng = gscore[u] + e.time_hours();
            if (ng < gscore[v]) {
                gscore[v] = ng;
                pred[v] = u;
                pred_edge[v] = e;
                open.push_or_decrease(v, ng + heuristic(v));
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    r.execution_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.expanded_vertices = expanded;

    if (gscore[dest] == INF) {
        r.success = false;
        r.status_message = "destination is unreachable from source";
        return r;
    }

    // Path reconstruction.
    std::vector<int>  path;
    std::vector<Edge> edges;
    int cur = dest;
    while (cur != -1) {
        path.push_back(cur);
        if (cur == source) break;
        edges.push_back(pred_edge[cur]);
        cur = pred[cur];
    }
    std::reverse(path.begin(), path.end());
    std::reverse(edges.begin(), edges.end());

    r.success = true;
    r.path = path;
    r.path_edges = edges;
    accumulate_metrics(g, edges, r);
    return r;
}

} // namespace freight
// ============================================================================
//  dijkstra.cpp — Implementation of Dijkstra's algorithm.
// ============================================================================
#include "dijkstra.hpp"
#include "avl_pq.hpp"
#include <chrono>
#include <algorithm>
#include <limits>

namespace freight {

RouteResult dijkstra(const Graph& g, int source, int dest, WeightMode mode) {
    RouteResult r;
    r.algorithm = "Dijkstra";

    const int n = g.num_vertices();
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        r.status_message = "invalid source or destination index";
        return r;
    }

    // Dijkstra's precondition: no negative edge weights.
    if (mode == WeightMode::Financial) {
        for (const Edge& e : g.edges()) {
            if (g.financial_cost(e) < 0.0) {
                r.success = false;
                r.status_message = "Dijkstra rejected the request: negative financial edge present "
                                   "(use Bellman-Ford for this objective)";
                return r;
            }
        }
    }

    auto t0 = std::chrono::steady_clock::now();

    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> dist(n, INF);
    std::vector<int>    pred(n, -1);
    std::vector<Edge>   pred_edge(n);
    std::vector<char>   settled(n, 0);

    AVLPriorityQueue pq;                 // AVL-backed indexed min-priority queue
    dist[source] = 0.0;
    pq.push_or_decrease(source, 0.0);

    int expanded = 0;
    while (!pq.empty()) {
        int u; double d;
        pq.pop_min(u, d);
        if (settled[u]) continue;
        settled[u] = 1;
        ++expanded;
        if (u == dest) break;

        for (const Edge& e : g.adj()[u]) {
            const int v = e.to;
            if (settled[v]) continue;
            const double w = g.edge_weight(e, mode);
            if (w < 0.0) continue;       // safety guard
            const double nd = dist[u] + w;
            if (nd < dist[v]) {
                dist[v] = nd;
                pred[v] = u;
                pred_edge[v] = e;
                pq.push_or_decrease(v, nd);   // decrease-key via AVL delete+insert
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    r.execution_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.expanded_vertices = expanded;

    if (dist[dest] == INF) {
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
// ============================================================================
//  prim.cpp — Prim's MST over the metric closure of the selected hubs.
// ============================================================================
#include "prim.hpp"
#include "dijkstra.hpp"
#include "avl_pq.hpp"
#include <chrono>
#include <algorithm>
#include <limits>

namespace freight {

MSTResult prim_mst(const Graph& g, const std::vector<int>& hubs_in) {
    MSTResult r;
    auto t0 = std::chrono::steady_clock::now();

    // Validate and de-duplicate hub ids.
    std::vector<int> hubs;
    for (int h : hubs_in) {
        if (h < 0 || h >= g.num_vertices()) {
            r.success = false;
            r.status_message = "invalid hub id supplied";
            return r;
        }
        if (std::find(hubs.begin(), hubs.end(), h) == hubs.end()) hubs.push_back(h);
    }
    r.hubs = hubs;
    const int k = (int)hubs.size();
    if (k < 2) {
        r.success = false;
        r.status_message = "select at least two distinct hubs to build a network";
        return r;
    }

    const double INF = std::numeric_limits<double>::infinity();

    // Metric closure: pairwise shortest-path distance between every hub pair.
    std::vector<std::vector<double>> metric(k, std::vector<double>(k, INF));
    for (int i = 0; i < k; ++i) metric[i][i] = 0.0;
    for (int i = 0; i < k; ++i) {
        for (int j = i + 1; j < k; ++j) {
            RouteResult rr = dijkstra(g, hubs[i], hubs[j], WeightMode::Distance);
            if (rr.success) metric[i][j] = metric[j][i] = rr.total_distance_km;
        }
    }

    // Prim's algorithm on the complete metric-closure graph (AVL-backed PQ).
    std::vector<char>   inMST(k, 0);
    std::vector<double> key(k, INF);
    std::vector<int>    parent(k, -1);

    AVLPriorityQueue pq;
    key[0] = 0.0;
    pq.push_or_decrease(0, 0.0);

    int added = 0;
    while (!pq.empty()) {
        int u; double w;
        pq.pop_min(u, w);
        if (inMST[u]) continue;
        inMST[u] = 1;
        ++added;
        for (int v = 0; v < k; ++v) {
            if (v == u || inMST[v]) continue;
            const double mw = metric[u][v];
            if (mw == INF) continue;
            if (mw < key[v]) {
                key[v] = mw;
                parent[v] = u;
                pq.push_or_decrease(v, mw);
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    r.execution_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // Disconnected-hub detection.
    if (added < k) {
        r.disconnected = true;
        for (int i = 0; i < k; ++i)
            if (!inMST[i]) r.unreachable_hubs.push_back(hubs[i]);
        r.success = false;
        r.status_message = "the selected hubs are not all mutually reachable; the network is disconnected";
        return r;
    }

    // Materialise each MST edge with its real road corridor (path + cost).
    r.total_distance_km = 0.0;
    r.total_cost = 0.0;
    for (int v = 1; v < k; ++v) {
        const int p = parent[v];
        if (p < 0) continue;
        MSTEdgeResult me;
        me.from_hub = hubs[p];
        me.to_hub   = hubs[v];
        RouteResult rr = dijkstra(g, hubs[p], hubs[v], WeightMode::Distance);
        if (rr.success) {
            me.distance_km    = rr.total_distance_km;
            me.cost           = rr.total_cost;
            me.corridor_path  = rr.path;
            me.corridor_edges = rr.path_edges;
        } else {
            me.distance_km = metric[p][v];
        }
        r.total_distance_km += me.distance_km;
        r.total_cost        += me.cost;
        r.mst_edges.push_back(me);
    }

    r.success = true;
    return r;
}

} // namespace freight
#include "prim.h"
#include "dijkstra.h"
#include "avl_pq.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

MSTResult freight_prim_mst(const Graph* g, const int* hubs_in, int num_hubs_in) {
    MSTResult r;
    mst_result_init(&r);
    double t0 = timer_now_ms();

    if (!hubs_in || num_hubs_in <= 0) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "select at least two distinct hubs to build a network");
        return r;
    }

    int* hubs = (int*)malloc((size_t)num_hubs_in * sizeof(int));
    int k = 0;

    for (int i = 0; i < num_hubs_in; ++i) {
        int h = hubs_in[i];
        if (h < 0 || h >= g->num_vertices) {
            r.success = false;
            snprintf(r.status_message, sizeof(r.status_message), "invalid hub id supplied");
            free(hubs);
            return r;
        }
        bool dup = false;
        for (int j = 0; j < k; ++j) {
            if (hubs[j] == h) { dup = true; break; }
        }
        if (!dup) hubs[k++] = h;
    }

    r.hubs     = hubs;
    r.num_hubs = k;

    if (k < 2) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "select at least two distinct hubs to build a network");
        return r;
    }

    /* Metric closure: k x k matrix */
    double* metric = (double*)malloc((size_t)(k * k) * sizeof(double));
    for (int i = 0; i < k * k; ++i) metric[i] = INFINITY;
    for (int i = 0; i < k; ++i) metric[i * k + i] = 0.0;

    for (int i = 0; i < k; ++i) {
        for (int j = i + 1; j < k; ++j) {
            RouteResult rr = freight_dijkstra(g, hubs[i], hubs[j], WEIGHT_DISTANCE);
            if (rr.success) {
                metric[i * k + j] = rr.total_distance_km;
                metric[j * k + i] = rr.total_distance_km;
            }
            route_result_free(&rr);
        }
    }

    char*   inMST  = (char*)calloc((size_t)k, sizeof(char));
    double* key    = (double*)malloc((size_t)k * sizeof(double));
    int*    parent = (int*)malloc((size_t)k * sizeof(int));
    for (int i = 0; i < k; ++i) {
        key[i]    = INFINITY;
        parent[i] = -1;
    }

    AVLPriorityQueue pq;
    avlpq_init(&pq, k);
    key[0] = 0.0;
    avlpq_push_or_decrease(&pq, 0, 0.0);

    int added = 0;
    while (!avlpq_empty(&pq)) {
        int u = -1;
        double w = 0.0;
        avlpq_pop_min(&pq, &u, &w);
        if (inMST[u]) continue;
        inMST[u] = 1;
        added++;

        for (int v = 0; v < k; ++v) {
            if (v == u || inMST[v]) continue;
            const double mw = metric[u * k + v];
            if (isinf(mw)) continue;
            if (mw < key[v]) {
                key[v]    = mw;
                parent[v] = u;
                avlpq_push_or_decrease(&pq, v, mw);
            }
        }
    }

    double t1 = timer_now_ms();
    r.execution_time_ms = t1 - t0;

    if (added < k) {
        r.disconnected = true;
        r.unreachable_hubs = (int*)malloc((size_t)(k - added) * sizeof(int));
        r.num_unreachable_hubs = 0;
        for (int i = 0; i < k; ++i) {
            if (!inMST[i]) r.unreachable_hubs[r.num_unreachable_hubs++] = hubs[i];
        }
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message),
                 "the selected hubs are not all mutually reachable; the network is disconnected");
        avlpq_free(&pq);
        free(metric); free(inMST); free(key); free(parent);
        return r;
    }

    r.mst_edges     = (MSTEdgeResult*)calloc((size_t)(k - 1), sizeof(MSTEdgeResult));
    r.num_mst_edges = 0;
    r.total_distance_km = 0.0;
    r.total_cost        = 0.0;

    for (int v = 1; v < k; ++v) {
        const int p = parent[v];
        if (p < 0) continue;
        MSTEdgeResult* me = &r.mst_edges[r.num_mst_edges++];
        me->from_hub = hubs[p];
        me->to_hub   = hubs[v];

        RouteResult rr = freight_dijkstra(g, hubs[p], hubs[v], WEIGHT_DISTANCE);
        if (rr.success) {
            me->distance_km        = rr.total_distance_km;
            me->cost               = rr.total_cost;
            me->corridor_path      = rr.path;
            me->corridor_path_len  = rr.path_len;
            me->corridor_edges     = rr.path_edges;
            me->corridor_edges_len = rr.path_edges_len;
            /* Ownership of path/path_edges transferred to me; do not free rr arrays */
        } else {
            me->distance_km = metric[p * k + v];
            route_result_free(&rr);
        }

        r.total_distance_km += me->distance_km;
        r.total_cost        += me->cost;
    }

    r.success = true;
    avlpq_free(&pq);
    free(metric); free(inMST); free(key); free(parent);
    return r;
}
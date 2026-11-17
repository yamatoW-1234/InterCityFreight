#include "astar.h"
#include "avl_pq.h"
#include "haversine.h"
#include "timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static double heuristic_hours(const Graph* g, int u, int dest) {
    const City* c    = &g->cities[u];
    const City* goal = &g->cities[dest];
    return geo_haversine_km(c->lat, c->lon, goal->lat, goal->lon) / g->max_speed;
}

RouteResult freight_astar(const Graph* g, int source, int dest) {
    RouteResult r;
    route_result_init(&r);
    snprintf(r.algorithm, sizeof(r.algorithm), "A* Search");

    const int n = g->num_vertices;
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "invalid source or destination index");
        return r;
    }

    double t0 = timer_now_ms();

    double* gscore    = (double*)malloc((size_t)n * sizeof(double));
    int*    pred      = (int*)malloc((size_t)n * sizeof(int));
    Edge*   pred_edge = (Edge*)calloc((size_t)n, sizeof(Edge));
    char*   closed    = (char*)calloc((size_t)n, sizeof(char));

    for (int i = 0; i < n; ++i) {
        gscore[i] = INFINITY;
        pred[i]   = -1;
    }

    AVLPriorityQueue open;
    avlpq_init(&open, n);
    gscore[source] = 0.0;
    avlpq_push_or_decrease(&open, source, heuristic_hours(g, source, dest));

    int expanded = 0;
    while (!avlpq_empty(&open)) {
        int u = -1;
        double f = 0.0;
        avlpq_pop_min(&open, &u, &f);
        if (closed[u]) continue;
        closed[u] = 1;
        expanded++;
        if (u == dest) break;

        const EdgeList* adj_u = &g->adj[u];
        for (int i = 0; i < adj_u->count; ++i) {
            const Edge* e = &adj_u->edges[i];
            const int v = e->to;
            if (closed[v]) continue;
            const double ng = gscore[u] + edge_time_hours(e);
            if (ng < gscore[v]) {
                gscore[v]    = ng;
                pred[v]      = u;
                pred_edge[v] = *e;
                avlpq_push_or_decrease(&open, v, ng + heuristic_hours(g, v, dest));
            }
        }
    }

    double t1 = timer_now_ms();
    r.execution_time_ms = t1 - t0;
    r.expanded_vertices = expanded;

    if (isinf(gscore[dest])) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "destination is unreachable from source");
        avlpq_free(&open);
        free(gscore); free(pred); free(pred_edge); free(closed);
        return r;
    }

    int count = 0;
    for (int cur = dest; cur != -1; cur = (cur == source ? -1 : pred[cur])) {
        count++;
    }

    r.path_len       = count;
    r.path_edges_len = count - 1;
    r.path           = (int*)malloc((size_t)r.path_len * sizeof(int));
    r.path_edges     = r.path_edges_len > 0 ? (Edge*)malloc((size_t)r.path_edges_len * sizeof(Edge)) : NULL;

    int idx = count - 1;
    int cur = dest;
    while (cur != -1) {
        r.path[idx] = cur;
        if (cur == source) break;
        r.path_edges[idx - 1] = pred_edge[cur];
        cur = pred[cur];
        idx--;
    }

    r.success = true;
    route_accumulate_metrics(g, &r);

    avlpq_free(&open);
    free(gscore); free(pred); free(pred_edge); free(closed);
    return r;
}
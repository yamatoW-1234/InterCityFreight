#include "bellman_ford.h"
#include "timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

RouteResult freight_bellman_ford(const Graph* g, int source, int dest) {
    RouteResult r;
    route_result_init(&r);
    snprintf(r.algorithm, sizeof(r.algorithm), "Bellman-Ford");

    const int n = g->num_vertices;
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "invalid source or destination index");
        return r;
    }

    double t0 = timer_now_ms();

    double* dist      = (double*)malloc((size_t)n * sizeof(double));
    int*    pred      = (int*)malloc((size_t)n * sizeof(int));
    Edge*   pred_edge = (Edge*)calloc((size_t)n, sizeof(Edge));

    for (int i = 0; i < n; ++i) {
        dist[i] = INFINITY;
        pred[i] = -1;
    }
    dist[source] = 0.0;

    const int m = g->num_directed_edges;
    long long relaxations = 0;

    for (int i = 0; i < n - 1; ++i) {
        bool changed = false;
        for (int j = 0; j < m; ++j) {
            const Edge* e = &g->edges[j];
            if (isinf(dist[e->from])) continue;
            const double nd = dist[e->from] + graph_financial_cost(g, e);
            if (nd < dist[e->to]) {
                dist[e->to]      = nd;
                pred[e->to]      = e->from;
                pred_edge[e->to] = *e;
                changed          = true;
                relaxations++;
            }
        }
        if (!changed) break;
    }

    char* affected = (char*)calloc((size_t)n, sizeof(char));
    for (int j = 0; j < m; ++j) {
        const Edge* e = &g->edges[j];
        if (!isinf(dist[e->from]) && dist[e->from] + graph_financial_cost(g, e) < dist[e->to]) {
            affected[e->to] = 1;
        }
    }
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < m; ++j) {
            const Edge* e = &g->edges[j];
            if (affected[e->from]) affected[e->to] = 1;
        }
    }

    double t1 = timer_now_ms();
    r.execution_time_ms = t1 - t0;
    r.expanded_vertices = (int)relaxations;

    if (affected[dest]) {
        r.success        = false;
        r.negative_cycle = true;
        snprintf(r.status_message, sizeof(r.status_message),
                 "a reachable negative-weight cycle affects the destination; "
                 "the minimum cost is unbounded and no finite route is returned");
        free(dist); free(pred); free(pred_edge); free(affected);
        return r;
    }

    if (isinf(dist[dest])) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "destination is unreachable from source");
        free(dist); free(pred); free(pred_edge); free(affected);
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

    free(dist); free(pred); free(pred_edge); free(affected);
    return r;
}
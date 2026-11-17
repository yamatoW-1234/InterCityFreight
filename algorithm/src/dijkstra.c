#include "dijkstra.h"
#include "avl_pq.h"
#include "timer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

RouteResult freight_dijkstra(const Graph* g, int source, int dest, WeightMode mode) {
    RouteResult r;
    route_result_init(&r);
    snprintf(r.algorithm, sizeof(r.algorithm), "Dijkstra");

    const int n = g->num_vertices;
    if (source < 0 || source >= n || dest < 0 || dest >= n) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "invalid source or destination index");
        return r;
    }

    if (mode == WEIGHT_FINANCIAL) {
        for (int i = 0; i < g->num_directed_edges; ++i) {
            if (graph_financial_cost(g, &g->edges[i]) < 0.0) {
                r.success = false;
                snprintf(r.status_message, sizeof(r.status_message),
                         "Dijkstra rejected the request: negative financial edge present "
                         "(use Bellman-Ford for this objective)");
                return r;
            }
        }
    }

    double t0 = timer_now_ms();

    double* dist      = (double*)malloc((size_t)n * sizeof(double));
    int*    pred      = (int*)malloc((size_t)n * sizeof(int));
    Edge*   pred_edge = (Edge*)calloc((size_t)n, sizeof(Edge));
    char*   settled   = (char*)calloc((size_t)n, sizeof(char));

    for (int i = 0; i < n; ++i) {
        dist[i] = INFINITY;
        pred[i] = -1;
    }

    AVLPriorityQueue pq;
    avlpq_init(&pq, n);
    dist[source] = 0.0;
    avlpq_push_or_decrease(&pq, source, 0.0);

    int expanded = 0;
    while (!avlpq_empty(&pq)) {
        int u = -1;
        double d = 0.0;
        avlpq_pop_min(&pq, &u, &d);
        if (settled[u]) continue;
        settled[u] = 1;
        expanded++;
        if (u == dest) break;

        const EdgeList* adj_u = &g->adj[u];
        for (int i = 0; i < adj_u->count; ++i) {
            const Edge* e = &adj_u->edges[i];
            const int v = e->to;
            if (settled[v]) continue;
            const double w = graph_edge_weight(g, e, mode);
            if (w < 0.0) continue;
            const double nd = dist[u] + w;
            if (nd < dist[v]) {
                dist[v]      = nd;
                pred[v]      = u;
                pred_edge[v] = *e;
                avlpq_push_or_decrease(&pq, v, nd);
            }
        }
    }

    double t1 = timer_now_ms();
    r.execution_time_ms = t1 - t0;
    r.expanded_vertices = expanded;

    if (isinf(dist[dest])) {
        r.success = false;
        snprintf(r.status_message, sizeof(r.status_message), "destination is unreachable from source");
        avlpq_free(&pq);
        free(dist); free(pred); free(pred_edge); free(settled);
        return r;
    }

    /* Count path length */
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

    avlpq_free(&pq);
    free(dist); free(pred); free(pred_edge); free(settled);
    return r;
}
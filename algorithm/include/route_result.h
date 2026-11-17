#ifndef ROUTE_RESULT_H
#define ROUTE_RESULT_H

#include <stdbool.h>
#include "graph.h"

#define MAX_ALGO_NAME_LEN 64
#define MAX_STATUS_LEN    256

typedef struct {
    bool   success;
    char   algorithm[MAX_ALGO_NAME_LEN];    /* "A* Search", "Dijkstra", "Bellman-Ford", ... */
    char   status_message[MAX_STATUS_LEN];  /* error / diagnostic detail when !success */

    int*   path;                            /* ordered city ids, source -> destination */
    int    path_len;

    Edge*  path_edges;                      /* directed edges used, parallel to `path` */
    int    path_edges_len;

    /* Operational metrics (Module 2 / synopsis §3) */
    double total_distance_km;
    double total_time_hours;
    double total_toll;
    double total_fuel;
    double total_subsidy;
    double total_cost;                      /* toll + fuel - subsidy */

    /* Algorithm performance statistics */
    int    expanded_vertices;
    double execution_time_ms;

    /* Bellman-Ford specific */
    bool   negative_cycle;
} RouteResult;

/* Lifecycle */
void route_result_init(RouteResult* r);
void route_result_free(RouteResult* r);

/* Sum every operational metric across r->path_edges */
static inline void route_accumulate_metrics(const Graph* g, RouteResult* r) {
    r->total_distance_km = 0.0;
    r->total_time_hours  = 0.0;
    r->total_toll        = 0.0;
    r->total_fuel        = 0.0;
    r->total_subsidy     = 0.0;
    r->total_cost        = 0.0;

    for (int i = 0; i < r->path_edges_len; ++i) {
        const Edge* e = &r->path_edges[i];
        r->total_distance_km += e->distance_km;
        r->total_time_hours  += edge_time_hours(e);
        r->total_toll        += e->toll;
        r->total_fuel        += graph_fuel_cost(g, e->distance_km);
        r->total_subsidy     += e->subsidy;
        r->total_cost        += graph_financial_cost(g, e);
    }
}

#endif /* ROUTE_RESULT_H */
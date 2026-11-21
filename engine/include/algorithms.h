#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "graph.h"

#define INF_COST 1e15

typedef struct {
    int found;
    int negative_cycle;
    int path_len;
    int path[MAX_CITIES];
    char highways[MAX_CITIES][MAX_HIGHWAY_LEN];
    double leg_arrival_hr[MAX_CITIES];
    double leg_wait_hr[MAX_CITIES];
    double total_distance_km;
    double total_time_hr;
    double total_toll_inr;
    double total_fuel_inr;
    double total_subsidy_inr;
    double net_cost_inr;
} PathResult;

typedef struct {
    int from;
    int to;
    char highway[MAX_HIGHWAY_LEN];
    double distance_km;
} MSTEdge;

typedef struct {
    int connected;
    int edge_count;
    MSTEdge edges[MAX_CITIES];
    double total_distance_km;
} MSTResult;

void path_result_init(PathResult *res);

PathResult run_astar(const Graph *g, const Vehicle *v, int src, int dst,
                     double departure_hr, double fuel_price_inr, int respect_windows);

PathResult run_dijkstra(const Graph *g, const Vehicle *v, int src, int dst,
                        double departure_hr, double fuel_price_inr, int by_cost);

PathResult run_bellman_ford(const Graph *g, const Vehicle *v, int src, int dst,
                            double departure_hr, double fuel_price_inr);

MSTResult run_prims_mst(const Graph *g, const int *hubs, int num_hubs);

#endif

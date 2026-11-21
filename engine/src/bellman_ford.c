#include "algorithms.h"
#include <string.h>

PathResult run_bellman_ford(const Graph *g, const Vehicle *v, int src, int dst,
                            double departure_hr, double fuel_price_inr) {
    PathResult res;
    path_result_init(&res);

    int n = g->num_cities;
    if (src < 0 || src >= n || dst < 0 || dst >= n) return res;
    if (g->avoid_city[src] || g->avoid_city[dst]) return res;

    double cost[MAX_CITIES];
    int parent[MAX_CITIES];
    const Edge *parent_edge[MAX_CITIES];

    for (int i = 0; i < MAX_CITIES; i++) {
        cost[i] = INF_COST;
        parent[i] = -1;
        parent_edge[i] = NULL;
    }
    cost[src] = 0.0;

    /* Relax all edges V - 1 times */
    for (int iter = 0; iter < n - 1; iter++) {
        int updated = 0;
        for (int u = 0; u < n; u++) {
            if (cost[u] >= INF_COST * 0.5 || g->avoid_city[u]) continue;
            for (const Edge *e = g->adj[u]; e != NULL; e = e->next) {
                int w_node = e->to;
                if (e->is_blocked || g->avoid_city[w_node]) continue;
                double w = edge_net_cost_inr(e, v, fuel_price_inr);
                if (cost[u] + w < cost[w_node] - 1e-9) {
                    cost[w_node] = cost[u] + w;
                    parent[w_node] = u;
                    parent_edge[w_node] = e;
                    updated = 1;
                }
            }
        }
        if (!updated) break;
    }

    /* Check for negative-weight cycles */
    for (int u = 0; u < n; u++) {
        if (cost[u] >= INF_COST * 0.5 || g->avoid_city[u]) continue;
        for (const Edge *e = g->adj[u]; e != NULL; e = e->next) {
            int w_node = e->to;
            if (e->is_blocked || g->avoid_city[w_node]) continue;
            double w = edge_net_cost_inr(e, v, fuel_price_inr);
            if (cost[u] + w < cost[w_node] - 1e-6) {
                res.negative_cycle = 1;
                return res;
            }
        }
    }

    if (cost[dst] >= INF_COST * 0.5) return res;
    res.found = 1;

    int rev_nodes[MAX_CITIES];
    const Edge *rev_edges[MAX_CITIES];
    int count = 0;
    for (int cur = dst; cur != -1 && count < MAX_CITIES; cur = parent[cur]) {
        rev_nodes[count] = cur;
        rev_edges[count] = parent_edge[cur];
        count++;
    }

    res.path_len = count;
    double cur_hr = departure_hr;
    double mileage = (v && v->mileage_kmpl > 0.1) ? v->mileage_kmpl : 3.5;
    double toll_mult = (v && v->toll_multiplier > 0.1) ? v->toll_multiplier : 1.0;

    for (int i = 0; i < count; i++) {
        int idx = count - 1 - i;
        res.path[i] = rev_nodes[idx];
        if (i == 0) {
            res.leg_arrival_hr[0] = cur_hr;
        } else if (rev_edges[idx]) {
            const Edge *e = rev_edges[idx];
            strncpy(res.highways[i - 1], e->highway, MAX_HIGHWAY_LEN - 1);
            double t_h = edge_travel_hours(e, v);
            cur_hr += t_h;
            res.leg_arrival_hr[i] = cur_hr;
            res.total_distance_km += e->distance_km;
            res.total_time_hr += t_h;
            res.total_toll_inr += e->toll_inr * toll_mult;
            res.total_fuel_inr += (e->distance_km / mileage) * fuel_price_inr;
            res.total_subsidy_inr += e->subsidy_inr;
        }
    }
    res.net_cost_inr = res.total_fuel_inr + res.total_toll_inr - res.total_subsidy_inr;
    return res;
}

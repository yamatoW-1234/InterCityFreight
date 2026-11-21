#include "algorithms.h"
#include "trees.h"
#include <string.h>

PathResult run_dijkstra(const Graph *g, const Vehicle *v, int src, int dst,
                        double departure_hr, double fuel_price_inr, int by_cost) {
    PathResult res;
    path_result_init(&res);

    if (src < 0 || src >= g->num_cities || dst < 0 || dst >= g->num_cities) return res;
    if (g->avoid_city[src] || g->avoid_city[dst]) return res;

    double dist[MAX_CITIES];
    double arrival_hr[MAX_CITIES];
    int parent[MAX_CITIES];
    const Edge *parent_edge[MAX_CITIES];
    int visited[MAX_CITIES];

    for (int i = 0; i < MAX_CITIES; i++) {
        dist[i] = INF_COST;
        arrival_hr[i] = departure_hr;
        parent[i] = -1;
        parent_edge[i] = NULL;
        visited[i] = 0;
    }

    AVLPriorityQueue pq;
    avl_pq_init(&pq);

    dist[src] = 0.0;
    avl_pq_push(&pq, 0.0, src, departure_hr);

    while (!avl_pq_empty(&pq)) {
        double d_u, t_u;
        int u;
        if (!avl_pq_pop_min(&pq, &d_u, &u, &t_u)) break;
        if (visited[u]) continue;
        visited[u] = 1;

        if (u == dst) {
            res.found = 1;
            break;
        }

        for (const Edge *e = g->adj[u]; e != NULL; e = e->next) {
            int nxt = e->to;
            if (e->is_blocked || g->avoid_city[nxt] || visited[nxt]) continue;

            double w = by_cost ? edge_net_cost_inr(e, v, fuel_price_inr) : e->distance_km;
            if (w < 0.0) w = 0.0; /* Dijkstra requires non-negative weights */

            double travel_h = edge_travel_hours(e, v);
            if (d_u + w < dist[nxt]) {
                dist[nxt] = d_u + w;
                arrival_hr[nxt] = t_u + travel_h;
                parent[nxt] = u;
                parent_edge[nxt] = e;
                avl_pq_push(&pq, dist[nxt], nxt, arrival_hr[nxt]);
            }
        }
    }

    avl_pq_free(&pq);
    if (!res.found) return res;

    int rev_nodes[MAX_CITIES];
    const Edge *rev_edges[MAX_CITIES];
    double rev_arr[MAX_CITIES];
    int count = 0;
    for (int cur = dst; cur != -1; cur = parent[cur]) {
        rev_nodes[count] = cur;
        rev_edges[count] = parent_edge[cur];
        rev_arr[count] = arrival_hr[cur];
        count++;
    }

    res.path_len = count;
    double mileage = (v && v->mileage_kmpl > 0.1) ? v->mileage_kmpl : 3.5;
    double toll_mult = (v && v->toll_multiplier > 0.1) ? v->toll_multiplier : 1.0;

    for (int i = 0; i < count; i++) {
        int idx = count - 1 - i;
        res.path[i] = rev_nodes[idx];
        res.leg_arrival_hr[i] = rev_arr[idx];
        if (i > 0 && rev_edges[idx]) {
            const Edge *e = rev_edges[idx];
            strncpy(res.highways[i - 1], e->highway, MAX_HIGHWAY_LEN - 1);
            res.total_distance_km += e->distance_km;
            res.total_time_hr += edge_travel_hours(e, v);
            res.total_toll_inr += e->toll_inr * toll_mult;
            res.total_fuel_inr += (e->distance_km / mileage) * fuel_price_inr;
            res.total_subsidy_inr += e->subsidy_inr;
        }
    }
    res.net_cost_inr = res.total_fuel_inr + res.total_toll_inr - res.total_subsidy_inr;
    return res;
}

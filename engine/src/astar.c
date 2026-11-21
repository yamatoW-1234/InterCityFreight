#include "algorithms.h"
#include "trees.h"
#include <string.h>

void path_result_init(PathResult *res) {
    memset(res, 0, sizeof(PathResult));
    res->found = 0;
    res->negative_cycle = 0;
    res->path_len = 0;
}

static double heuristic_hours(const Graph *g, const Vehicle *v, int u, int dst) {
    double dist = haversine_km(g->cities[u].lat, g->cities[u].lon,
                               g->cities[dst].lat, g->cities[dst].lon);
    double max_spd = (v && v->max_speed_kmh > 0.0) ? v->max_speed_kmh : 80.0;
    return dist / max_spd;
}

PathResult run_astar(const Graph *g, const Vehicle *v, int src, int dst,
                     double departure_hr, double fuel_price_inr, int respect_windows) {
    PathResult res;
    path_result_init(&res);

    if (src < 0 || src >= g->num_cities || dst < 0 || dst >= g->num_cities) return res;
    if (g->avoid_city[src] || g->avoid_city[dst]) return res;

    double g_score[MAX_CITIES];
    double arrival_hr[MAX_CITIES];
    double wait_hr[MAX_CITIES];
    int parent[MAX_CITIES];
    const Edge *parent_edge[MAX_CITIES];
    int closed[MAX_CITIES];

    for (int i = 0; i < MAX_CITIES; i++) {
        g_score[i] = INF_COST;
        arrival_hr[i] = departure_hr;
        wait_hr[i] = 0.0;
        parent[i] = -1;
        parent_edge[i] = NULL;
        closed[i] = 0;
    }

    AVLPriorityQueue pq;
    avl_pq_init(&pq);

    g_score[src] = 0.0;
    arrival_hr[src] = departure_hr;
    avl_pq_push(&pq, heuristic_hours(g, v, src, dst), src, departure_hr);

    while (!avl_pq_empty(&pq)) {
        double f_curr, t_curr;
        int u;
        if (!avl_pq_pop_min(&pq, &f_curr, &u, &t_curr)) break;
        (void)f_curr;
        if (closed[u]) continue;
        closed[u] = 1;

        if (u == dst) {
            res.found = 1;
            break;
        }

        for (const Edge *e = g->adj[u]; e != NULL; e = e->next) {
            int nxt = e->to;
            if (e->is_blocked || g->avoid_city[nxt] || closed[nxt]) continue;

            double wait_h = 0.0;
            if (respect_windows) {
                edge_is_open_at(e, t_curr, &wait_h);
            }
            double travel_h = edge_travel_hours(e, v);
            double tentative_g = g_score[u] + wait_h + travel_h;

            if (tentative_g < g_score[nxt]) {
                g_score[nxt] = tentative_g;
                arrival_hr[nxt] = t_curr + wait_h + travel_h;
                wait_hr[nxt] = wait_h;
                parent[nxt] = u;
                parent_edge[nxt] = e;
                double f_nxt = tentative_g + heuristic_hours(g, v, nxt, dst);
                avl_pq_push(&pq, f_nxt, nxt, arrival_hr[nxt]);
            }
        }
    }

    avl_pq_free(&pq);
    if (!res.found) return res;

    int rev_nodes[MAX_CITIES];
    const Edge *rev_edges[MAX_CITIES];
    double rev_arr[MAX_CITIES];
    double rev_wait[MAX_CITIES];
    int count = 0;
    for (int cur = dst; cur != -1; cur = parent[cur]) {
        rev_nodes[count] = cur;
        rev_edges[count] = parent_edge[cur];
        rev_arr[count] = arrival_hr[cur];
        rev_wait[count] = wait_hr[cur];
        count++;
    }

    res.path_len = count;
    double mileage = (v && v->mileage_kmpl > 0.1) ? v->mileage_kmpl : 3.5;
    double toll_mult = (v && v->toll_multiplier > 0.1) ? v->toll_multiplier : 1.0;

    for (int i = 0; i < count; i++) {
        int idx = count - 1 - i;
        res.path[i] = rev_nodes[idx];
        res.leg_arrival_hr[i] = rev_arr[idx];
        res.leg_wait_hr[i] = rev_wait[idx];
        if (i > 0 && rev_edges[idx]) {
            const Edge *e = rev_edges[idx];
            strncpy(res.highways[i - 1], e->highway, MAX_HIGHWAY_LEN - 1);
            res.total_distance_km += e->distance_km;
            res.total_toll_inr += e->toll_inr * toll_mult;
            res.total_fuel_inr += (e->distance_km / mileage) * fuel_price_inr;
            res.total_subsidy_inr += e->subsidy_inr;
        }
    }
    res.total_time_hr = g_score[dst];
    res.net_cost_inr = res.total_fuel_inr + res.total_toll_inr - res.total_subsidy_inr;
    return res;
}

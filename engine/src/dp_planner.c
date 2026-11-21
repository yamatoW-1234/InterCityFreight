#include "dp_knapsack.h"
#include <stdlib.h>
#include <string.h>

static PathResult solve_leg(const Graph *g, const Vehicle *v, int src, int dst,
                            double dep_hr, double fuel_price_inr,
                            const char *optimize_for, int respect_windows) {
    if (strcmp(optimize_for, "cheapest") == 0) {
        return run_bellman_ford(g, v, src, dst, dep_hr, fuel_price_inr);
    } else if (strcmp(optimize_for, "shortest") == 0) {
        return run_dijkstra(g, v, src, dst, dep_hr, fuel_price_inr, 0);
    }
    return run_astar(g, v, src, dst, dep_hr, fuel_price_inr, respect_windows);
}

static double leg_metric(const PathResult *p, const char *optimize_for) {
    if (!p->found) return INF_COST;
    if (strcmp(optimize_for, "cheapest") == 0) return p->net_cost_inr;
    if (strcmp(optimize_for, "shortest") == 0) return p->total_distance_km;
    return p->total_time_hr;
}

/*
 * Held-Karp Bitmask Dynamic Programming over K mandatory stops (K <= MAX_STOPS):
 * Finds the optimal visit order: origin -> perm(stops) -> destination,
 * then stitches full paths and time-windowed itinerary arrival/wait hours.
 */
PathResult run_dp_stop_planner(const Graph *g, const Vehicle *v,
                               int origin, int destination,
                               const int *stops, int num_stops,
                               double departure_hr, double fuel_price_inr,
                               const char *optimize_for, int respect_windows) {
    PathResult final_res;
    path_result_init(&final_res);

    if (num_stops <= 0) {
        return solve_leg(g, v, origin, destination, departure_hr, fuel_price_inr, optimize_for, respect_windows);
    }
    if (num_stops > MAX_STOPS) num_stops = MAX_STOPS;

    /* Node list: 0 = origin, 1..K = stops, K+1 = destination */
    int k = num_stops;
    int nodes[MAX_STOPS + 2];
    nodes[0] = origin;
    for (int i = 0; i < k; i++) nodes[i + 1] = stops[i];
    nodes[k + 1] = destination;
    int total_pts = k + 2;

    /* Precompute pairwise leg costs */
    double cost_mat[MAX_STOPS + 2][MAX_STOPS + 2];
    for (int i = 0; i < total_pts; i++) {
        for (int j = 0; j < total_pts; j++) {
            if (i == j) {
                cost_mat[i][j] = 0.0;
            } else {
                PathResult p = solve_leg(g, v, nodes[i], nodes[j], departure_hr, fuel_price_inr, optimize_for, 0);
                cost_mat[i][j] = leg_metric(&p, optimize_for);
            }
        }
    }

    int full_mask = (1 << k);
    double *dp = (double *)malloc((size_t)full_mask * (size_t)k * sizeof(double));
    int *parent = (int *)malloc((size_t)full_mask * (size_t)k * sizeof(int));
    if (!dp || !parent) {
        free(dp);
        free(parent);
        return final_res;
    }

    for (int m = 0; m < full_mask; m++) {
        for (int i = 0; i < k; i++) {
            dp[m * k + i] = INF_COST;
            parent[m * k + i] = -1;
        }
    }

    /* Base cases: origin -> stop i */
    for (int i = 0; i < k; i++) {
        dp[(1 << i) * k + i] = cost_mat[0][i + 1];
    }

    /* DP transitions */
    for (int mask = 1; mask < full_mask; mask++) {
        for (int last = 0; last < k; last++) {
            if (!(mask & (1 << last))) continue;
            double cur_val = dp[mask * k + last];
            if (cur_val >= INF_COST * 0.5) continue;

            for (int nxt = 0; nxt < k; nxt++) {
                if (mask & (1 << nxt)) continue;
                int nmask = mask | (1 << nxt);
                double cand = cur_val + cost_mat[last + 1][nxt + 1];
                if (cand < dp[nmask * k + nxt]) {
                    dp[nmask * k + nxt] = cand;
                    parent[nmask * k + nxt] = last;
                }
            }
        }
    }

    /* Pick best last stop -> destination */
    int last_best = -1;
    double best_total = INF_COST;
    int all_visited = full_mask - 1;
    for (int i = 0; i < k; i++) {
        double cand = dp[all_visited * k + i] + cost_mat[i + 1][k + 1];
        if (cand < best_total) {
            best_total = cand;
            last_best = i;
        }
    }

    if (last_best == -1 || best_total >= INF_COST * 0.5) {
        free(dp);
        free(parent);
        return final_res;
    }

    /* Reconstruct stop order */
    int stop_order[MAX_STOPS];
    int cur_mask = all_visited;
    int cur_stop = last_best;
    for (int idx = k - 1; idx >= 0; idx--) {
        stop_order[idx] = stops[cur_stop];
        int prev = parent[cur_mask * k + cur_stop];
        cur_mask ^= (1 << cur_stop);
        cur_stop = prev;
    }

    free(dp);
    free(parent);

    /* Stitch exact legs sequentially while tracking live arrival & wait times */
    int seq[MAX_STOPS + 2];
    seq[0] = origin;
    for (int i = 0; i < k; i++) seq[i + 1] = stop_order[i];
    seq[k + 1] = destination;

    double cur_hr = departure_hr;
    final_res.found = 1;
    final_res.path[0] = origin;
    final_res.leg_arrival_hr[0] = cur_hr;
    final_res.leg_wait_hr[0] = 0.0;
    final_res.path_len = 1;

    for (int s = 0; s < k + 1; s++) {
        PathResult seg = solve_leg(g, v, seq[s], seq[s + 1], cur_hr, fuel_price_inr, optimize_for, respect_windows);
        if (!seg.found) {
            final_res.found = 0;
            return final_res;
        }
        for (int j = 1; j < seg.path_len; j++) {
            if (final_res.path_len >= MAX_CITIES) break;
            int out_idx = final_res.path_len;
            final_res.path[out_idx] = seg.path[j];
            final_res.leg_arrival_hr[out_idx] = seg.leg_arrival_hr[j];
            final_res.leg_wait_hr[out_idx] = seg.leg_wait_hr[j];
            strncpy(final_res.highways[out_idx - 1], seg.highways[j - 1], MAX_HIGHWAY_LEN - 1);
            final_res.path_len++;
        }
        final_res.total_distance_km += seg.total_distance_km;
        final_res.total_time_hr += seg.total_time_hr;
        final_res.total_toll_inr += seg.total_toll_inr;
        final_res.total_fuel_inr += seg.total_fuel_inr;
        final_res.total_subsidy_inr += seg.total_subsidy_inr;
        cur_hr = seg.leg_arrival_hr[seg.path_len - 1];
    }
    final_res.net_cost_inr = final_res.total_fuel_inr + final_res.total_toll_inr - final_res.total_subsidy_inr;
    return final_res;
}

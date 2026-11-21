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

    int k = num_stops;
    int nodes[MAX_STOPS + 2];
    nodes[0] = origin;
    for (int i = 0; i < k; i++) nodes[i + 1] = stops[i];
    nodes[k + 1] = destination;
    int total_pts = k + 2;

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

    for (int i = 0; i < k; i++) {
        dp[(1 << i) * k + i] = cost_mat[0][i + 1];
    }

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
        if (seq[s] == seq[s + 1]) continue;
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

static void add_stop_unique(int *stops, int *count, int city_id, int final_dest) {
    if (city_id == final_dest || *count >= MAX_STOPS) return;
    for (int i = 0; i < *count; i++) {
        if (stops[i] == city_id) return;
    }
    stops[(*count)++] = city_id;
}

static void remove_stop(int *stops, int *count, int city_id) {
    for (int i = 0; i < *count; i++) {
        if (stops[i] == city_id) {
            for (int j = i; j < *count - 1; j++) stops[j] = stops[j + 1];
            (*count)--;
            return;
        }
    }
}

static void append_cargo_event(DynamicCargoLog *log, int city_id, const char *action,
                               const char *item_name, int dest_city_id,
                               double weight_kg, double fraction, double value_inr) {
    if (!log || log->event_count >= MAX_CARGO_EVENTS) return;
    CargoEvent *ev = &log->events[log->event_count++];
    ev->city_id = city_id;
    strncpy(ev->action, action, sizeof(ev->action) - 1);
    strncpy(ev->item_name, item_name, sizeof(ev->item_name) - 1);
    ev->dest_city_id = dest_city_id;
    ev->weight_kg = weight_kg;
    ev->fraction = fraction;
    ev->value_inr = value_inr;
}

PathResult run_dynamic_cargo_route(const Graph *g, const Vehicle *v,
                                   int origin, int destination,
                                   const int *initial_stops, int num_initial_stops,
                                   CargoItem *warehouse_items, int num_items,
                                   double departure_hr, double fuel_price_inr,
                                   const char *optimize_for, int respect_windows,
                                   DynamicCargoLog *log_out) {
    PathResult res;
    path_result_init(&res);
    if (log_out) memset(log_out, 0, sizeof(DynamicCargoLog));

    int active_stops[MAX_STOPS];
    int active_count = 0;
    for (int i = 0; i < num_initial_stops && i < MAX_STOPS; i++) {
        add_stop_unique(active_stops, &active_count, initial_stops[i], destination);
    }

    int picked_flag[MAX_CARGO_ITEMS] = {0};
    CargoItem onboard[MAX_CARGO_ITEMS];
    int onboard_count = 0;

    double max_cap = (v && v->cargo_capacity_kg > 0.0) ? v->cargo_capacity_kg : 25000.0;
    double current_load = 0.0;
    int cur_city = origin;
    double cur_hr = departure_hr;

    res.found = 1;
    res.path[0] = origin;
    res.leg_arrival_hr[0] = cur_hr;
    res.leg_wait_hr[0] = 0.0;
    res.path_len = 1;

    double mileage = (v && v->mileage_kmpl > 0.1) ? v->mileage_kmpl : 3.5;
    double toll_mult = (v && v->toll_multiplier > 0.1) ? v->toll_multiplier : 1.0;

    for (int step = 0; step < MAX_CITIES - 1; step++) {
        /* 1. Drop off any onboard cargo destined for cur_city */
        for (int i = 0; i < onboard_count; ) {
            if (onboard[i].dest_city_id == cur_city) {
                current_load -= onboard[i].taken_kg;
                if (current_load < 0.0) current_load = 0.0;
                append_cargo_event(log_out, cur_city, "dropoff", onboard[i].item_name,
                                   onboard[i].dest_city_id, onboard[i].taken_kg,
                                   onboard[i].fraction, onboard[i].allocated_value_inr);
                for (int j = i; j < onboard_count - 1; j++) onboard[j] = onboard[j + 1];
                onboard_count--;
            } else {
                i++;
            }
        }
        remove_stop(active_stops, &active_count, cur_city);

        /* 2. Inspect ONLY warehouse cargos located at cur_city */
        CargoItem local_items[MAX_CARGO_ITEMS];
        int local_indices[MAX_CARGO_ITEMS];
        int local_count = 0;
        for (int i = 0; i < num_items && i < MAX_CARGO_ITEMS; i++) {
            if (picked_flag[i]) continue;
            if (warehouse_items[i].src_city_id != cur_city) continue;
            if (g->avoid_city[warehouse_items[i].dest_city_id]) continue;
            local_items[local_count] = warehouse_items[i];
            local_indices[local_count] = i;
            local_count++;
        }

        double free_cap = max_cap - current_load;
        if (local_count > 0 && free_cap > 1.0) {
            KnapsackResult k_res = run_partial_knapsack(local_items, local_count, free_cap);
            for (int i = 0; i < k_res.item_count; i++) {
                CargoItem *chosen = &k_res.items[i];
                if (chosen->taken_kg <= 0.0) continue;

                /* Mark original warehouse item picked */
                for (int m = 0; m < local_count; m++) {
                    int orig_idx = local_indices[m];
                    if (!picked_flag[orig_idx] &&
                        strcmp(warehouse_items[orig_idx].item_name, chosen->item_name) == 0 &&
                        warehouse_items[orig_idx].dest_city_id == chosen->dest_city_id) {
                        picked_flag[orig_idx] = 1;
                        break;
                    }
                }

                current_load += chosen->taken_kg;
                if (log_out) {
                    if (current_load > log_out->peak_load_kg) log_out->peak_load_kg = current_load;
                    log_out->total_cargo_value_inr += chosen->allocated_value_inr;
                }
                if (onboard_count < MAX_CARGO_ITEMS) {
                    onboard[onboard_count++] = *chosen;
                }
                append_cargo_event(log_out, cur_city, "pickup", chosen->item_name,
                                   chosen->dest_city_id, chosen->taken_kg,
                                   chosen->fraction, chosen->allocated_value_inr);
                add_stop_unique(active_stops, &active_count, chosen->dest_city_id, destination);
            }
        }

        /* 3. Done if we are at destination with no pending dropoffs/stops */
        if (cur_city == destination && active_count == 0) {
            break;
        }

        /* 4. Replan remaining route via Held-Karp DP + A-star/Bellman-Ford/Dijkstra */
        PathResult plan = run_dp_stop_planner(g, v, cur_city, destination,
                                              active_stops, active_count,
                                              cur_hr, fuel_price_inr,
                                              optimize_for, respect_windows);
        if (!plan.found || plan.path_len < 2) {
            res.found = 0;
            return res;
        }

        /* Advance one hop along the replanned route */
        int nxt = plan.path[1];
        const Edge *chosen_edge = NULL;
        for (const Edge *e = g->adj[cur_city]; e != NULL; e = e->next) {
            if (!e->is_blocked && e->to == nxt) {
                chosen_edge = e;
                break;
            }
        }

        if (chosen_edge) {
            double wait_h = 0.0;
            if (respect_windows) edge_is_open_at(chosen_edge, cur_hr, &wait_h);
            double trav_h = edge_travel_hours(chosen_edge, v);
            cur_hr += wait_h + trav_h;

            res.path[res.path_len] = nxt;
            res.leg_arrival_hr[res.path_len] = cur_hr;
            res.leg_wait_hr[res.path_len] = wait_h;
            strncpy(res.highways[res.path_len - 1], chosen_edge->highway, MAX_HIGHWAY_LEN - 1);
            res.path_len++;

            res.total_distance_km += chosen_edge->distance_km;
            res.total_time_hr += wait_h + trav_h;
            res.total_toll_inr += chosen_edge->toll_inr * toll_mult;
            res.total_fuel_inr += (chosen_edge->distance_km / mileage) * fuel_price_inr;
            res.total_subsidy_inr += chosen_edge->subsidy_inr;
        } else {
            res.found = 0;
            return res;
        }
        cur_city = nxt;
    }

    res.net_cost_inr = res.total_fuel_inr + res.total_toll_inr - res.total_subsidy_inr;
    return res;
}
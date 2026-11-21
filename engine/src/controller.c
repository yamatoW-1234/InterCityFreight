#include "controller.h"
#include "algorithms.h"
#include "dp_knapsack.h"
#include "trees.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Lightweight, zero-dependency JSON helpers for CargoCompass stdin/stdout contract.
 * Reads full JSON from stdin, builds Graph + BTree + Vehicle + CargoItems,
 * routes to the optimal DAA pipeline, and prints JSON to stdout.
 */

static char *read_all_stdin(size_t *len_out) {
    size_t cap = 65536;
    size_t len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = getchar()) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb) {
                free(buf);
                return NULL;
            }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    if (len_out) *len_out = len;
    return buf;
}

static const char *skip_ws(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static const char *find_matching_bracket(const char *p, char open_c, char close_c) {
    if (!p || *p != open_c) return NULL;
    int depth = 0, in_str = 0;
    while (*p) {
        if (*p == '"' && (p == NULL || *(p - 1) != '\\')) in_str = !in_str;
        if (!in_str) {
            if (*p == open_c) depth++;
            else if (*p == close_c) {
                depth--;
                if (depth == 0) return p;
            }
        }
        p++;
    }
    return NULL;
}

static const char *find_key_in_range(const char *start, const char *end, const char *key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    size_t plen = strlen(pat);
    const char *p = start;
    int depth = 0, in_str = 0;
    while (p < end) {
        if (*p == '"' && (p == start || *(p - 1) != '\\')) {
            if (!in_str && depth == 1 && (size_t)(end - p) >= plen && strncmp(p, pat, plen) == 0) {
                const char *after = skip_ws(p + plen);
                if (after < end && *after == ':') return skip_ws(after + 1);
            }
            in_str = !in_str;
        } else if (!in_str) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
        }
        p++;
    }
    return NULL;
}

static int parse_int_val(const char *p, int def) {
    if (!p) return def;
    p = skip_ws(p);
    if (*p == 't') return 1;
    if (*p == 'f') return 0;
    if (!isdigit((unsigned char)*p) && *p != '-') return def;
    return atoi(p);
}

static double parse_double_val(const char *p, double def) {
    if (!p) return def;
    p = skip_ws(p);
    if (!isdigit((unsigned char)*p) && *p != '-' && *p != '.') return def;
    return atof(p);
}

static void parse_str_val(const char *p, char *out, size_t max_len) {
    out[0] = '\0';
    if (!p) return;
    p = skip_ws(p);
    if (*p != '"') return;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < max_len) {
        if (*p == '\\' && *(p + 1)) p++;
        out[i++] = *p++;
    }
    out[i] = '\0';
}

static void parse_cities_array(const char *arr_start, const char *arr_end, Graph *g, BTree *btree) {
    const char *p = arr_start;
    while (p < arr_end) {
        if (*p == '{') {
            const char *obj_end = find_matching_bracket(p, '{', '}');
            if (!obj_end || obj_end > arr_end) break;
            int id = parse_int_val(find_key_in_range(p, obj_end, "id"), -1);
            char name[64], state[48];
            parse_str_val(find_key_in_range(p, obj_end, "name"), name, sizeof(name));
            parse_str_val(find_key_in_range(p, obj_end, "state"), state, sizeof(state));
            double lat = parse_double_val(find_key_in_range(p, obj_end, "lat"), 0.0);
            double lon = parse_double_val(find_key_in_range(p, obj_end, "lon"), 0.0);
            if (id >= 0 && id < MAX_CITIES) {
                graph_add_city(g, id, name, state, lat, lon);
                btree_insert(btree, id, &g->cities[id]);
            }
            p = obj_end + 1;
        } else {
            p++;
        }
    }
}

static void parse_roads_array(const char *arr_start, const char *arr_end, Graph *g) {
    const char *p = arr_start;
    while (p < arr_end) {
        if (*p == '{') {
            const char *obj_end = find_matching_bracket(p, '{', '}');
            if (!obj_end || obj_end > arr_end) break;
            Edge e;
            memset(&e, 0, sizeof(Edge));
            e.from = parse_int_val(find_key_in_range(p, obj_end, "from"), -1);
            e.to = parse_int_val(find_key_in_range(p, obj_end, "to"), -1);
            parse_str_val(find_key_in_range(p, obj_end, "highway"), e.highway, sizeof(e.highway));
            e.distance_km = parse_double_val(find_key_in_range(p, obj_end, "distance_km"), 0.0);
            e.speed_limit_kmh = parse_double_val(find_key_in_range(p, obj_end, "speed_limit_kmh"), 70.0);
            e.toll_inr = parse_double_val(find_key_in_range(p, obj_end, "toll_inr"), 0.0);
            e.subsidy_inr = parse_double_val(find_key_in_range(p, obj_end, "subsidy_inr"), 0.0);
            e.traffic_multiplier = parse_double_val(find_key_in_range(p, obj_end, "traffic_multiplier"), 1.0);
            e.open_hour = parse_int_val(find_key_in_range(p, obj_end, "open_hour"), 0);
            e.close_hour = parse_int_val(find_key_in_range(p, obj_end, "close_hour"), 24);
            e.is_blocked = parse_int_val(find_key_in_range(p, obj_end, "is_blocked"), 0);
            graph_add_edge(g, &e);
            p = obj_end + 1;
        } else {
            p++;
        }
    }
}

static int parse_int_array(const char *p, const char *root_end, int *out, int max_count) {
    if (!p) return 0;
    p = skip_ws(p);
    if (*p != '[') return 0;
    const char *arr_end = find_matching_bracket(p, '[', ']');
    if (!arr_end || arr_end > root_end) return 0;
    int count = 0;
    p++;
    while (p < arr_end && count < max_count) {
        p = skip_ws(p);
        if (isdigit((unsigned char)*p) || *p == '-') {
            out[count++] = atoi(p);
            while (p < arr_end && (isdigit((unsigned char)*p) || *p == '-')) p++;
        } else {
            p++;
        }
    }
    return count;
}

static int parse_cargo_array(const char *arr_start, const char *arr_end, CargoItem *items, int max_items) {
    int count = 0;
    const char *p = arr_start;
    while (p < arr_end && count < max_items) {
        if (*p == '{') {
            const char *obj_end = find_matching_bracket(p, '{', '}');
            if (!obj_end || obj_end > arr_end) break;
            CargoItem *it = &items[count++];
            memset(it, 0, sizeof(CargoItem));
            it->city_id = parse_int_val(find_key_in_range(p, obj_end, "city_id"), 0);
            it->src_city_id = parse_int_val(find_key_in_range(p, obj_end, "src_city_id"), it->city_id);
            it->dest_city_id = parse_int_val(find_key_in_range(p, obj_end, "dest_city_id"), it->city_id);
            parse_str_val(find_key_in_range(p, obj_end, "type"), it->type, sizeof(it->type));
            parse_str_val(find_key_in_range(p, obj_end, "item_name"), it->item_name, sizeof(it->item_name));
            if (it->item_name[0] == '\0') {
                parse_str_val(find_key_in_range(p, obj_end, "name"), it->item_name, sizeof(it->item_name));
            }
            parse_str_val(find_key_in_range(p, obj_end, "action"), it->action, sizeof(it->action));
            if (it->action[0] == '\0') strcpy(it->action, "pickup");
            it->weight_kg = parse_double_val(find_key_in_range(p, obj_end, "weight_kg"), 0.0);
            it->value_inr = parse_double_val(find_key_in_range(p, obj_end, "value_inr"), 0.0);
            p = obj_end + 1;
        } else {
            p++;
        }
    }
    return count;
}

static void print_json_escaped(const char *s) {
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

int run_decision_controller(void) {
    clock_t t_start = clock();

    size_t len = 0;
    char *input = read_all_stdin(&len);
    if (!input || len == 0) {
        printf("{\"error\":\"Empty stdin payload\"}\n");
        free(input);
        return 1;
    }

    const char *root_start = skip_ws(input);
    const char *root_end = find_matching_bracket(root_start, '{', '}');
    if (!root_end) {
        printf("{\"error\":\"Invalid JSON object\"}\n");
        free(input);
        return 1;
    }

    Graph g;
    BTree city_index;
    graph_init(&g);
    btree_init(&city_index);

    const char *cities_val = find_key_in_range(root_start, root_end, "cities");
    if (cities_val && *cities_val == '[') {
        const char *cend = find_matching_bracket(cities_val, '[', ']');
        if (cend) parse_cities_array(cities_val, cend, &g, &city_index);
    }

    const char *roads_val = find_key_in_range(root_start, root_end, "roads");
    if (roads_val && *roads_val == '[') {
        const char *rend = find_matching_bracket(roads_val, '[', ']');
        if (rend) parse_roads_array(roads_val, rend, &g);
    }

    Vehicle veh = {.id = "default", .name = "Standard Truck", .mileage_kmpl = 3.5,
                   .cargo_capacity_kg = 25000.0, .fuel_capacity_l = 350.0,
                   .max_speed_kmh = 80.0, .toll_multiplier = 1.0, .toll_axle_class = 3};
    const char *veh_val = find_key_in_range(root_start, root_end, "vehicle");
    if (veh_val && *veh_val == '{') {
        const char *vend = find_matching_bracket(veh_val, '{', '}');
        if (vend) {
            parse_str_val(find_key_in_range(veh_val, vend, "id"), veh.id, sizeof(veh.id));
            parse_str_val(find_key_in_range(veh_val, vend, "name"), veh.name, sizeof(veh.name));
            veh.mileage_kmpl = parse_double_val(find_key_in_range(veh_val, vend, "mileage_kmpl"), 3.5);
            veh.cargo_capacity_kg = parse_double_val(find_key_in_range(veh_val, vend, "cargo_capacity_kg"), 25000.0);
            veh.fuel_capacity_l = parse_double_val(find_key_in_range(veh_val, vend, "fuel_capacity_l"), 350.0);
            veh.max_speed_kmh = parse_double_val(find_key_in_range(veh_val, vend, "max_speed_kmh"), 80.0);
            veh.toll_multiplier = parse_double_val(find_key_in_range(veh_val, vend, "toll_multiplier"), 1.0);
            veh.toll_axle_class = parse_int_val(find_key_in_range(veh_val, vend, "toll_axle_class"), 3);
        }
    }

    int origin = parse_int_val(find_key_in_range(root_start, root_end, "origin"), 0);
    int destination = parse_int_val(find_key_in_range(root_start, root_end, "destination"), 1);
    double departure_hr = parse_double_val(find_key_in_range(root_start, root_end, "departure_hour"), 8.0);
    double fuel_price = parse_double_val(find_key_in_range(root_start, root_end, "fuel_price_inr"), 95.0);
    int respect_windows = parse_int_val(find_key_in_range(root_start, root_end, "respect_time_windows"), 1);
    int dynamic_cargo = parse_int_val(find_key_in_range(root_start, root_end, "dynamic_cargo"), 0);

    char optimize_for[32] = "fastest";
    const char *opt_val = find_key_in_range(root_start, root_end, "optimize_for");
    if (opt_val) parse_str_val(opt_val, optimize_for, sizeof(optimize_for));

    int avoid_list[MAX_CITIES];
    int avoid_count = parse_int_array(find_key_in_range(root_start, root_end, "avoid_cities"), root_end, avoid_list, MAX_CITIES);
    for (int i = 0; i < avoid_count; i++) {
        if (avoid_list[i] >= 0 && avoid_list[i] < MAX_CITIES) g.avoid_city[avoid_list[i]] = 1;
    }

    int stops[MAX_STOPS];
    int num_stops = parse_int_array(find_key_in_range(root_start, root_end, "stops"), root_end, stops, MAX_STOPS);

    int hubs[MAX_CITIES];
    int num_hubs = parse_int_array(find_key_in_range(root_start, root_end, "hubs"), root_end, hubs, MAX_CITIES);

    CargoItem cargo_items[MAX_CARGO_ITEMS];
    int cargo_count = 0;
    const char *cargo_val = find_key_in_range(root_start, root_end, "cargo_items");
    if (cargo_val && *cargo_val == '[') {
        const char *cend = find_matching_bracket(cargo_val, '[', ']');
        if (cend) cargo_count = parse_cargo_array(cargo_val, cend, cargo_items, MAX_CARGO_ITEMS);
    }

    /* Internal Decision Controller: select optimal algorithm pipeline */
    const char *algo_used = "A* Search (AVL Priority Queue)";
    MSTResult mst_res;
    PathResult path_res;
    memset(&mst_res, 0, sizeof(MSTResult));
    path_result_init(&path_res);

    DynamicCargoLog cargo_log;
    memset(&cargo_log, 0, sizeof(DynamicCargoLog));
    int is_mst_mode = (strcmp(optimize_for, "mst") == 0 && num_hubs >= 2);
    if (is_mst_mode) {
        algo_used = "Prim's MST (AVL Priority Queue)";
        mst_res = run_prims_mst(&g, hubs, num_hubs);
    } else if (dynamic_cargo && cargo_count > 0) {
        algo_used = "Dynamic Cargo (Partial Knapsack + Held-Karp DP)";
        path_res = run_dynamic_cargo_route(&g, &veh, origin, destination, stops, num_stops,
                                           cargo_items, cargo_count, departure_hr, fuel_price,
                                           optimize_for, respect_windows, &cargo_log);
    } else if (num_stops > 0) {
        if (strcmp(optimize_for, "cheapest") == 0) {
            algo_used = "Held-Karp Bitmask DP + Bellman-Ford";
        } else if (strcmp(optimize_for, "shortest") == 0) {
            algo_used = "Held-Karp Bitmask DP + Dijkstra";
        } else {
            algo_used = "Held-Karp Bitmask DP + A* Search";
        }
        path_res = run_dp_stop_planner(&g, &veh, origin, destination, stops, num_stops,
                                       departure_hr, fuel_price, optimize_for, respect_windows);
    } else if (strcmp(optimize_for, "cheapest") == 0) {
        algo_used = "Bellman-Ford (Cost & Rebate Optimization)";
        path_res = run_bellman_ford(&g, &veh, origin, destination, departure_hr, fuel_price);
    } else if (strcmp(optimize_for, "shortest") == 0) {
        algo_used = "Dijkstra (AVL Priority Queue)";
        path_res = run_dijkstra(&g, &veh, origin, destination, departure_hr, fuel_price, 0);
    } else {
        algo_used = "A* Search (AVL Priority Queue)";
        path_res = run_astar(&g, &veh, origin, destination, departure_hr, fuel_price, respect_windows);
    }

    KnapsackResult knap_res = run_partial_knapsack(cargo_items, cargo_count, veh.cargo_capacity_kg);

    clock_t t_end = clock();
    double runtime_ms = ((double)(t_end - t_start) / CLOCKS_PER_SEC) * 1000.0;

    /* Emit JSON output */
    printf("{\n");
    printf("  \"algorithm_used\": "); print_json_escaped(algo_used); printf(",\n");
    printf("  \"runtime_ms\": %.3f,\n", runtime_ms);

    if (is_mst_mode) {
        printf("  \"mode\": \"mst\",\n");
        printf("  \"connected\": %s,\n", mst_res.connected ? "true" : "false");
        printf("  \"total_distance_km\": %.1f,\n", mst_res.total_distance_km);
        printf("  \"mst_edges\": [\n");
        for (int i = 0; i < mst_res.edge_count; i++) {
            City *c1 = btree_search(&city_index, mst_res.edges[i].from);
            City *c2 = btree_search(&city_index, mst_res.edges[i].to);
            printf("    {\"from\": %d, \"from_name\": ", mst_res.edges[i].from);
            print_json_escaped(c1 ? c1->name : "");
            printf(", \"to\": %d, \"to_name\": ", mst_res.edges[i].to);
            print_json_escaped(c2 ? c2->name : "");
            printf(", \"highway\": ");
            print_json_escaped(mst_res.edges[i].highway);
            printf(", \"distance_km\": %.1f}%s\n", mst_res.edges[i].distance_km,
                   (i + 1 < mst_res.edge_count) ? "," : "");
        }
        printf("  ],\n");
    } else {
        printf("  \"mode\": \"route\",\n");
        printf("  \"found\": %s,\n", path_res.found ? "true" : "false");
        printf("  \"negative_cycle\": %s,\n", path_res.negative_cycle ? "true" : "false");
        printf("  \"total_distance_km\": %.1f,\n", path_res.total_distance_km);
        printf("  \"total_time_hr\": %.2f,\n", path_res.total_time_hr);
        printf("  \"total_toll_inr\": %.2f,\n", path_res.total_toll_inr);
        printf("  \"total_fuel_inr\": %.2f,\n", path_res.total_fuel_inr);
        printf("  \"total_subsidy_inr\": %.2f,\n", path_res.total_subsidy_inr);
        printf("  \"net_cost_inr\": %.2f,\n", path_res.net_cost_inr);
        printf("  \"itinerary\": [\n");
        for (int i = 0; i < path_res.path_len; i++) {
            City *c = btree_search(&city_index, path_res.path[i]);
            printf("    {\"city_id\": %d, \"name\": ", path_res.path[i]);
            print_json_escaped(c ? c->name : "");
            printf(", \"lat\": %.4f, \"lon\": %.4f, \"arrival_hour\": %.2f, \"wait_hours\": %.2f, \"via_highway\": ",
                   c ? c->lat : 0.0, c ? c->lon : 0.0,
                   path_res.leg_arrival_hr[i], path_res.leg_wait_hr[i]);
            print_json_escaped(i > 0 ? path_res.highways[i - 1] : "ORIGIN");
            printf("}%s\n", (i + 1 < path_res.path_len) ? "," : "");
        }
        printf("  ],\n");
    }

    printf("  \"cargo_events\": [\n");
    for (int i = 0; i < cargo_log.event_count; i++) {
        const CargoEvent *ev = &cargo_log.events[i];
        City *c = btree_search(&city_index, ev->city_id);
        City *dc = btree_search(&city_index, ev->dest_city_id);
        printf("    {\"city_id\": %d, \"city_name\": ", ev->city_id);
        print_json_escaped(c ? c->name : "");
        printf(", \"action\": "); print_json_escaped(ev->action);
        printf(", \"item_name\": "); print_json_escaped(ev->item_name);
        printf(", \"dest_city_id\": %d, \"dest_city_name\": ", ev->dest_city_id);
        print_json_escaped(dc ? dc->name : "");
        printf(", \"weight_kg\": %.1f, \"fraction\": %.3f, \"value_inr\": %.2f}%s\n",
               ev->weight_kg, ev->fraction, ev->value_inr,
               (i + 1 < cargo_log.event_count) ? "," : "");
    }
    printf("  ],\n");
    printf("  \"knapsack\": {\n");
    printf("    \"capacity_kg\": %.1f,\n", knap_res.capacity_kg);
    printf("    \"used_kg\": %.1f,\n", knap_res.used_kg);
    printf("    \"total_value_inr\": %.2f,\n", knap_res.total_value_inr);
    printf("    \"allocations\": [\n");
    for (int i = 0; i < knap_res.item_count; i++) {
        const CargoItem *it = &knap_res.items[i];
        City *c = btree_search(&city_index, it->city_id);
        printf("      {\"city_id\": %d, \"city_name\": ", it->city_id);
        print_json_escaped(c ? c->name : "");
        printf(", \"item_name\": "); print_json_escaped(it->item_name);
        printf(", \"action\": "); print_json_escaped(it->action);
        printf(", \"weight_kg\": %.1f, \"taken_kg\": %.1f, \"fraction\": %.3f, \"allocated_value_inr\": %.2f}%s\n",
               it->weight_kg, it->taken_kg, it->fraction, it->allocated_value_inr,
               (i + 1 < knap_res.item_count) ? "," : "");
    }
    printf("    ]\n");
    printf("  }\n");
    printf("}\n");

    btree_free(&city_index);
    graph_free(&g);
    free(input);
    return 0;
}
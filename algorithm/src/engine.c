#include "engine.h"
#include "astar.h"
#include "dijkstra.h"
#include "bellman_ford.h"
#include "prim.h"
#include "avl_tree.h"
#include "haversine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void engine_init_struct(Engine* eng) {
    graph_init(&eng->graph);
    btree_init(&eng->city_index, 3);
    eng->data_dir[0] = '\0';
}

void engine_free(Engine* eng) {
    graph_free(&eng->graph);
    btree_free(&eng->city_index);
}

bool engine_load(Engine* eng, const char* data_dir, char* err_buf, size_t err_size) {
    strncpy(eng->data_dir, data_dir, MAX_PATH_LEN - 1);
    eng->data_dir[MAX_PATH_LEN - 1] = '\0';

    if (!graph_load(&eng->graph, data_dir, err_buf, err_size)) return false;

    btree_free(&eng->city_index);
    btree_init(&eng->city_index, 3);
    for (int i = 0; i < eng->graph.num_vertices; ++i) {
        btree_insert(&eng->city_index, eng->graph.cities[i].name, eng->graph.cities[i].id);
    }
    return true;
}

static bool resolve_city(const Engine* eng, const char* name, int* out_id) {
    if (!name || name[0] == '\0') return false;
    return btree_search(&eng->city_index, name, out_id);
}

static RouteResult run_objective(const Engine* eng, const char* objective, int source, int dest,
                                 const char** out_algo, const char** out_reason) {
    if (strcmp(objective, "fastest") == 0) {
        *out_algo   = "A* Search";
        *out_reason = "Fastest-transit objective: A* uses travel-time edge weights with an "
                      "admissible Haversine straight-line heuristic.";
        return freight_astar(&eng->graph, source, dest);
    }
    if (strcmp(objective, "shortest") == 0) {
        *out_algo   = "Dijkstra";
        *out_reason = "Shortest-distance objective: Dijkstra with non-negative distance weights.";
        return freight_dijkstra(&eng->graph, source, dest, WEIGHT_DISTANCE);
    }
    if (graph_has_negative_financial_edge(&eng->graph)) {
        *out_algo   = "Bellman-Ford";
        *out_reason = "Minimum-cost objective with negative net-cost edges (subsidies) present: "
                      "Bellman-Ford handles negative weights and detects negative cycles.";
        return freight_bellman_ford(&eng->graph, source, dest);
    }
    *out_algo   = "Dijkstra";
    *out_reason = "Minimum-cost objective with all non-negative costs: Dijkstra is the most "
                  "efficient valid choice (Bellman-Ford would also be correct).";
    return freight_dijkstra(&eng->graph, source, dest, WEIGHT_FINANCIAL);
}

static cJSON* route_to_json(const Engine* eng, const RouteResult* r, const char* objective,
                            const char* algorithm, const char* reason,
                            const char* src_name, const char* dst_name,
                            double budget, double time_limit) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "objective", objective);
    cJSON_AddStringToObject(o, "algorithm", r->algorithm[0] ? r->algorithm : algorithm);
    cJSON_AddStringToObject(o, "selection_reason", reason);

    if (!r->success) {
        cJSON_AddStringToObject(o, "status", "error");
        cJSON_AddStringToObject(o, "message", r->status_message);
        cJSON_AddBoolToObject(o, "negative_cycle", r->negative_cycle);
        return o;
    }

    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddStringToObject(o, "source", src_name);
    cJSON_AddStringToObject(o, "destination", dst_name);

    cJSON* pathArr  = cJSON_CreateArray();
    cJSON* coordArr = cJSON_CreateArray();
    for (int i = 0; i < r->path_len; ++i) {
        int id = r->path[i];
        cJSON_AddItemToArray(pathArr, cJSON_CreateString(eng->graph.cities[id].name));
        cJSON* c = cJSON_CreateArray();
        cJSON_AddItemToArray(c, cJSON_CreateNumber(eng->graph.cities[id].lat));
        cJSON_AddItemToArray(c, cJSON_CreateNumber(eng->graph.cities[id].lon));
        cJSON_AddItemToArray(coordArr, c);
    }
    cJSON_AddItemToObject(o, "path", pathArr);
    cJSON_AddItemToObject(o, "path_coords", coordArr);

    cJSON* edgeArr = cJSON_CreateArray();
    for (int i = 0; i < r->path_edges_len; ++i) {
        const Edge* e = &r->path_edges[i];
        cJSON* ej = cJSON_CreateObject();
        cJSON_AddStringToObject(ej, "from", eng->graph.cities[e->from].name);
        cJSON_AddStringToObject(ej, "to", eng->graph.cities[e->to].name);
        cJSON_AddStringToObject(ej, "highway", e->highway);
        cJSON_AddNumberToObject(ej, "distance_km", e->distance_km);
        cJSON_AddNumberToObject(ej, "time_hours", edge_time_hours(e));
        cJSON_AddNumberToObject(ej, "toll", e->toll);
        cJSON_AddNumberToObject(ej, "fuel", graph_fuel_cost(&eng->graph, e->distance_km));
        cJSON_AddNumberToObject(ej, "subsidy", e->subsidy);
        cJSON_AddNumberToObject(ej, "cost", graph_financial_cost(&eng->graph, e));
        cJSON_AddItemToArray(edgeArr, ej);
    }
    cJSON_AddItemToObject(o, "edges", edgeArr);

    cJSON_AddNumberToObject(o, "total_distance_km", r->total_distance_km);
    cJSON_AddNumberToObject(o, "total_time_hours", r->total_time_hours);
    cJSON_AddNumberToObject(o, "total_toll", r->total_toll);
    cJSON_AddNumberToObject(o, "total_fuel", r->total_fuel);
    cJSON_AddNumberToObject(o, "total_subsidy", r->total_subsidy);
    cJSON_AddNumberToObject(o, "total_cost", r->total_cost);
    cJSON_AddNumberToObject(o, "execution_time_ms", r->execution_time_ms);
    cJSON_AddNumberToObject(o, "expanded_vertices", r->expanded_vertices);
    cJSON_AddBoolToObject(o, "negative_cycle", r->negative_cycle);

    cJSON_AddNumberToObject(o, "budget_limit", budget);
    cJSON_AddNumberToObject(o, "time_limit_hours", time_limit);
    cJSON_AddBoolToObject(o, "within_budget", budget > 0 ? (r->total_cost <= budget) : true);
    cJSON_AddBoolToObject(o, "within_time", time_limit > 0 ? (r->total_time_hours <= time_limit) : true);
    return o;
}

static cJSON* error_json(const char* msg) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "error");
    cJSON_AddStringToObject(o, "message", msg);
    return o;
}

static cJSON* handle_route(const Engine* eng, const cJSON* req) {
    const cJSON* jsrc = cJSON_GetObjectItemCaseSensitive(req, "source");
    const cJSON* jdst = cJSON_GetObjectItemCaseSensitive(req, "destination");
    const cJSON* jobj = cJSON_GetObjectItemCaseSensitive(req, "objective");
    const cJSON* jbud = cJSON_GetObjectItemCaseSensitive(req, "budget_limit");
    const cJSON* jtim = cJSON_GetObjectItemCaseSensitive(req, "time_limit_hours");

    const char* src       = cJSON_IsString(jsrc) ? jsrc->valuestring : "";
    const char* dst       = cJSON_IsString(jdst) ? jdst->valuestring : "";
    const char* objective = cJSON_IsString(jobj) ? jobj->valuestring : "fastest";
    double budget         = cJSON_IsNumber(jbud) ? jbud->valuedouble : 0.0;
    double time_limit     = cJSON_IsNumber(jtim) ? jtim->valuedouble : 0.0;

    char err[256];
    int s = -1, d = -1;
    if (!resolve_city(eng, src, &s)) {
        snprintf(err, sizeof(err), "unknown source city: %s", src);
        return error_json(err);
    }
    if (!resolve_city(eng, dst, &d)) {
        snprintf(err, sizeof(err), "unknown destination city: %s", dst);
        return error_json(err);
    }

    const char* algo = "";
    const char* reason = "";
    RouteResult r = run_objective(eng, objective, s, d, &algo, &reason);
    cJSON* out = route_to_json(eng, &r, objective, algo, reason, src, dst, budget, time_limit);
    route_result_free(&r);
    return out;
}

static cJSON* handle_compare(const Engine* eng, const cJSON* req) {
    const cJSON* jsrc = cJSON_GetObjectItemCaseSensitive(req, "source");
    const cJSON* jdst = cJSON_GetObjectItemCaseSensitive(req, "destination");
    const char* src = cJSON_IsString(jsrc) ? jsrc->valuestring : "";
    const char* dst = cJSON_IsString(jdst) ? jdst->valuestring : "";

    char err[256];
    int s = -1, d = -1;
    if (!resolve_city(eng, src, &s)) {
        snprintf(err, sizeof(err), "unknown source city: %s", src);
        return error_json(err);
    }
    if (!resolve_city(eng, dst, &d)) {
        snprintf(err, sizeof(err), "unknown destination city: %s", dst);
        return error_json(err);
    }

    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddStringToObject(o, "source", src);
    cJSON_AddStringToObject(o, "destination", dst);

    cJSON* routes = cJSON_CreateArray();
    const char* objectives[3] = {"fastest", "cheapest", "shortest"};
    for (int i = 0; i < 3; ++i) {
        const char* algo = "";
        const char* reason = "";
        RouteResult r = run_objective(eng, objectives[i], s, d, &algo, &reason);
        cJSON_AddItemToArray(routes, route_to_json(eng, &r, objectives[i], algo, reason, src, dst, 0.0, 0.0));
        route_result_free(&r);
    }
    cJSON_AddItemToObject(o, "routes", routes);
    return o;
}

static cJSON* handle_mst(const Engine* eng, const cJSON* req) {
    const cJSON* jhubs = cJSON_GetObjectItemCaseSensitive(req, "hubs");
    if (!cJSON_IsArray(jhubs)) {
        return error_json("mst requires a 'hubs' array of city names");
    }

    int count = cJSON_GetArraySize(jhubs);
    int* hubs = count > 0 ? (int*)malloc((size_t)count * sizeof(int)) : NULL;
    int idx = 0;

    const cJSON* h = NULL;
    cJSON_ArrayForEach(h, jhubs) {
        const char* name = cJSON_IsString(h) ? h->valuestring : "";
        int id = -1;
        if (!resolve_city(eng, name, &id)) {
            char err[256];
            snprintf(err, sizeof(err), "unknown hub city: %s", name);
            free(hubs);
            return error_json(err);
        }
        hubs[idx++] = id;
    }

    MSTResult m = freight_prim_mst(&eng->graph, hubs, idx);
    free(hubs);

    cJSON* o = cJSON_CreateObject();
    cJSON* hubArr = cJSON_CreateArray();
    for (int i = 0; i < m.num_hubs; ++i) {
        cJSON_AddItemToArray(hubArr, cJSON_CreateString(eng->graph.cities[m.hubs[i]].name));
    }
    cJSON_AddItemToObject(o, "hubs", hubArr);
    cJSON_AddStringToObject(o, "status", m.success ? "ok" : "error");

    if (!m.success) {
        cJSON_AddStringToObject(o, "message", m.status_message);
        cJSON_AddBoolToObject(o, "disconnected", m.disconnected);
        cJSON* un = cJSON_CreateArray();
        for (int i = 0; i < m.num_unreachable_hubs; ++i) {
            cJSON_AddItemToArray(un, cJSON_CreateString(eng->graph.cities[m.unreachable_hubs[i]].name));
        }
        cJSON_AddItemToObject(o, "unreachable_hubs", un);
        mst_result_free(&m);
        return o;
    }

    cJSON* edgeArr = cJSON_CreateArray();
    for (int i = 0; i < m.num_mst_edges; ++i) {
        const MSTEdgeResult* me = &m.mst_edges[i];
        cJSON* ej = cJSON_CreateObject();
        cJSON_AddStringToObject(ej, "from", eng->graph.cities[me->from_hub].name);
        cJSON_AddStringToObject(ej, "to", eng->graph.cities[me->to_hub].name);
        cJSON_AddNumberToObject(ej, "distance_km", me->distance_km);
        cJSON_AddNumberToObject(ej, "cost", me->cost);

        cJSON* cp = cJSON_CreateArray();
        cJSON* cc = cJSON_CreateArray();
        for (int j = 0; j < me->corridor_path_len; ++j) {
            int cid = me->corridor_path[j];
            cJSON_AddItemToArray(cp, cJSON_CreateString(eng->graph.cities[cid].name));
            cJSON* c = cJSON_CreateArray();
            cJSON_AddItemToArray(c, cJSON_CreateNumber(eng->graph.cities[cid].lat));
            cJSON_AddItemToArray(c, cJSON_CreateNumber(eng->graph.cities[cid].lon));
            cJSON_AddItemToArray(cc, c);
        }
        cJSON_AddItemToObject(ej, "corridor_path", cp);
        cJSON_AddItemToObject(ej, "corridor_coords", cc);
        cJSON_AddItemToArray(edgeArr, ej);
    }
    cJSON_AddItemToObject(o, "mst_edges", edgeArr);
    cJSON_AddNumberToObject(o, "total_distance_km", m.total_distance_km);
    cJSON_AddNumberToObject(o, "total_cost", m.total_cost);
    cJSON_AddNumberToObject(o, "execution_time_ms", m.execution_time_ms);
    cJSON_AddBoolToObject(o, "disconnected", false);

    mst_result_free(&m);
    return o;
}

static cJSON* handle_network(const Engine* eng) {
    cJSON* o = graph_to_summary_json(&eng->graph);
    cJSON_AddStringToObject(o, "status", "ok");
    return o;
}

static void add_algo_meta(cJSON* arr, const char* name, const char* purpose, const char* weight,
                          const char* heur, const char* best, const char* avg, const char* worst,
                          const char* space, const char* ds) {
    cJSON* a = cJSON_CreateObject();
    cJSON_AddStringToObject(a, "name", name);
    cJSON_AddStringToObject(a, "purpose", purpose);
    cJSON_AddStringToObject(a, "weight", weight);
    cJSON_AddStringToObject(a, "heuristic", heur);
    cJSON_AddStringToObject(a, "time_best", best);
    cJSON_AddStringToObject(a, "time_avg", avg);
    cJSON_AddStringToObject(a, "time_worst", worst);
    cJSON_AddStringToObject(a, "space", space);
    cJSON_AddStringToObject(a, "data_structure", ds);
    cJSON_AddItemToArray(arr, a);
}

static cJSON* handle_meta(const Engine* eng) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddStringToObject(o, "project_name", "Inter-City Freight Logistics & Route Generator");
    cJSON_AddStringToObject(o, "subject", "Design and Analysis of Algorithms (DAA)");
    cJSON_AddNumberToObject(o, "num_cities", eng->graph.num_vertices);
    cJSON_AddNumberToObject(o, "num_roads", graph_num_undirected_roads(&eng->graph));
    cJSON_AddBoolToObject(o, "has_negative_financial_edge", graph_has_negative_financial_edge(&eng->graph));

    cJSON* algos = cJSON_CreateArray();
    add_algo_meta(algos, "A* Search", "Fastest freight transit",
                  "estimated travel time (distance / speed)",
                  "Haversine straight-line distance / max dataset speed (admissible + consistent)",
                  "O(E)", "O((V+E) log V)", "O((V+E) log V)", "O(V)",
                  "AVL-backed indexed min-priority queue");
    add_algo_meta(algos, "Dijkstra", "Shortest distance / non-negative minimum cost",
                  "distance, or non-negative financial cost",
                  "none (greedy best-first expansion)",
                  "O((V+E) log V)", "O((V+E) log V)", "O((V+E) log V)", "O(V)",
                  "AVL-backed indexed min-priority queue");
    add_algo_meta(algos, "Bellman-Ford", "Minimum financial cost with possible negative edges (subsidies)",
                  "toll + fuel - subsidy",
                  "none (dynamic programming over edge relaxations)",
                  "O(V x E)", "O(V x E)", "O(V x E)", "O(V)",
                  "distance + predecessor arrays");
    add_algo_meta(algos, "Prim's MST", "Multi-hub minimum-cost infrastructure backbone",
                  "shortest-path corridor distance (metric closure)",
                  "none (greedy nearest-hub attachment)",
                  "O((V+E) log V)", "O((V+E) log V)", "O((V+E) log V)", "O(V)",
                  "AVL-backed indexed min-priority queue");
    cJSON_AddItemToObject(o, "algorithms", algos);

    cJSON* ds = cJSON_CreateArray();
    cJSON* d1 = cJSON_CreateObject();
    cJSON_AddStringToObject(d1, "name", "AVL Tree");
    cJSON_AddStringToObject(d1, "role", "backs the indexed min-priority queue used by A*, Dijkstra and Prim");
    cJSON_AddStringToObject(d1, "ops", "insert / delete / search / extract-min / decrease-key in O(log n) via rotations");
    cJSON_AddItemToArray(ds, d1);

    cJSON* d2 = cJSON_CreateObject();
    cJSON_AddStringToObject(d2, "name", "B-Tree");
    cJSON_AddStringToObject(d2, "role", "city-name -> city-id index; resolves every request city via a B-Tree search");
    cJSON_AddStringToObject(d2, "ops", "insert-with-split / search / traversal in O(log n), configurable min degree");
    cJSON_AddItemToArray(ds, d2);

    cJSON_AddItemToObject(o, "data_structures", ds);
    return o;
}

static double lcg_rand(unsigned long long* state) {
    *state = (*state) * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(((*state) >> 33) & 0x7fffffff) / (double)0x7fffffff;
}

static void syn_add_road(Graph* g, int a, int b, double dist, double speed, double toll) {
    if (a == b) return;
    if (g->num_directed_edges + 2 > g->edge_capacity) {
        int new_cap = g->edge_capacity == 0 ? 64 : g->edge_capacity * 2;
        g->edges = (Edge*)realloc(g->edges, (size_t)new_cap * sizeof(Edge));
        g->edge_capacity = new_cap;
    }
    Edge fwd = {a, b, "SYN-HW", dist, speed, toll, 0.0};
    Edge rev = {b, a, "SYN-HW", dist, speed, toll, 0.0};
    g->edges[g->num_directed_edges++] = fwd;
    g->edges[g->num_directed_edges++] = rev;

    /* Push into adjacency lists */
    for (int dir = 0; dir < 2; ++dir) {
        Edge e = (dir == 0) ? fwd : rev;
        EdgeList* list = &g->adj[e.from];
        if (list->count >= list->capacity) {
            int nc = list->capacity == 0 ? 4 : list->capacity * 2;
            list->edges = (Edge*)realloc(list->edges, (size_t)nc * sizeof(Edge));
            list->capacity = nc;
        }
        list->edges[list->count++] = e;
    }
}

static void build_synthetic_graph_in_memory(Graph* g, int n) {
    graph_init(g);
    unsigned long long state = 123456789ULL + (unsigned long long)n * 1000003ULL;

    g->cities        = (City*)calloc((size_t)n, sizeof(City));
    g->city_capacity = n;
    g->num_vertices  = n;
    g->adj           = (EdgeList*)calloc((size_t)n, sizeof(EdgeList));

    for (int i = 0; i < n; ++i) {
        g->cities[i].id  = i;
        snprintf(g->cities[i].name, MAX_CITY_NAME_LEN, "City%d", i);
        g->cities[i].lat = 8.0 + lcg_rand(&state) * 27.0;
        g->cities[i].lon = 68.0 + lcg_rand(&state) * 29.0;
    }

    for (int i = 0; i + 1 < n; ++i) {
        double d = geo_haversine_km(g->cities[i].lat, g->cities[i].lon,
                                    g->cities[i + 1].lat, g->cities[i + 1].lon);
        if (d < 1.0) d = 1.0;
        double spd = 60.0 + lcg_rand(&state) * 30.0;
        syn_add_road(g, i, i + 1, d, spd, d * (2.0 + lcg_rand(&state) * 2.0));
    }
    for (int e = 0; e < n; ++e) {
        int a = (int)(lcg_rand(&state) * n); if (a >= n) a = n - 1;
        int b = (int)(lcg_rand(&state) * n); if (b >= n) b = n - 1;
        if (a == b) continue;
        double d = geo_haversine_km(g->cities[a].lat, g->cities[a].lon,
                                    g->cities[b].lat, g->cities[b].lon);
        if (d < 1.0) d = 1.0;
        double spd = 60.0 + lcg_rand(&state) * 30.0;
        syn_add_road(g, a, b, d, spd, d * (2.0 + lcg_rand(&state) * 2.0));
    }

    g->max_speed = 90.0;
}

static cJSON* handle_benchmark(const cJSON* req) {
    int default_sizes[5] = {10, 25, 50, 100, 200};
    int* sizes = default_sizes;
    int num_sizes = 5;
    int* custom_sizes = NULL;

    const cJSON* jsizes = cJSON_GetObjectItemCaseSensitive(req, "sizes");
    if (cJSON_IsArray(jsizes) && cJSON_GetArraySize(jsizes) > 0) {
        num_sizes = cJSON_GetArraySize(jsizes);
        custom_sizes = (int*)malloc((size_t)num_sizes * sizeof(int));
        int idx = 0;
        const cJSON* s = NULL;
        cJSON_ArrayForEach(s, jsizes) {
            custom_sizes[idx++] = cJSON_IsNumber(s) ? s->valueint : 0;
        }
        sizes = custom_sizes;
    }

    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON* results = cJSON_CreateArray();

    for (int i = 0; i < num_sizes; ++i) {
        int n = sizes[i];
        if (n < 3) continue;
        Graph g;
        build_synthetic_graph_in_memory(&g, n);
        const int src = 0, dst = n - 1;

        RouteResult rd = freight_dijkstra(&g, src, dst, WEIGHT_DISTANCE);
        RouteResult ra = freight_astar(&g, src, dst);
        RouteResult rb = freight_bellman_ford(&g, src, dst);
        int hubs[4] = {0, n / 3, (2 * n) / 3, n - 1};
        MSTResult mp = freight_prim_mst(&g, hubs, 4);

        cJSON* row = cJSON_CreateObject();
        cJSON_AddNumberToObject(row, "vertices", n);
        cJSON_AddNumberToObject(row, "edges", graph_num_undirected_roads(&g));
        cJSON_AddNumberToObject(row, "dijkstra_ms", rd.execution_time_ms);
        cJSON_AddNumberToObject(row, "astar_ms", ra.execution_time_ms);
        cJSON_AddNumberToObject(row, "bellman_ford_ms", rb.execution_time_ms);
        cJSON_AddNumberToObject(row, "prim_ms", mp.execution_time_ms);
        cJSON_AddNumberToObject(row, "dijkstra_expanded", rd.expanded_vertices);
        cJSON_AddNumberToObject(row, "astar_expanded", ra.expanded_vertices);
        cJSON_AddItemToArray(results, row);

        route_result_free(&rd);
        route_result_free(&ra);
        route_result_free(&rb);
        mst_result_free(&mp);
        graph_free(&g);
    }

    free(custom_sizes);
    cJSON_AddItemToObject(o, "results", results);
    return o;
}

static cJSON* handle_avl_demo(const cJSON* req) {
    AVLTree tree;
    avl_init(&tree);
    cJSON* steps = cJSON_CreateArray();

    const cJSON* jins = cJSON_GetObjectItemCaseSensitive(req, "insert");
    if (cJSON_IsArray(jins)) {
        const cJSON* v = NULL;
        cJSON_ArrayForEach(v, jins) {
            if (cJSON_IsNumber(v)) {
                PQEntry e = {(double)v->valueint, -1, v->valueint};
                avl_insert(&tree, e);
                cJSON_AddItemToArray(steps, avl_structure_json(&tree));
            }
        }
    }

    const cJSON* jdel = cJSON_GetObjectItemCaseSensitive(req, "delete");
    if (cJSON_IsArray(jdel)) {
        const cJSON* v = NULL;
        cJSON_ArrayForEach(v, jdel) {
            if (cJSON_IsNumber(v)) {
                PQEntry e = {(double)v->valueint, -1, v->valueint};
                avl_remove(&tree, e);
                cJSON_AddItemToArray(steps, avl_structure_json(&tree));
            }
        }
    }

    size_t sz = avl_size(&tree);
    PQEntry* arr = sz > 0 ? (PQEntry*)malloc(sz * sizeof(PQEntry)) : NULL;
    size_t cnt = sz > 0 ? avl_inorder(&tree, arr) : 0;

    cJSON* inorder = cJSON_CreateArray();
    for (size_t i = 0; i < cnt; ++i) {
        cJSON_AddItemToArray(inorder, cJSON_CreateNumber(arr[i].id));
    }
    free(arr);

    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddItemToObject(o, "steps", steps);
    cJSON_AddItemToObject(o, "final", avl_structure_json(&tree));
    cJSON_AddItemToObject(o, "inorder", inorder);
    cJSON_AddNumberToObject(o, "height", avl_height(&tree));
    cJSON_AddNumberToObject(o, "size", (double)sz);
    cJSON_AddBoolToObject(o, "valid", avl_is_valid(&tree));

    avl_clear(&tree);
    return o;
}

static cJSON* handle_btree_demo(const cJSON* req) {
    const cJSON* jt = cJSON_GetObjectItemCaseSensitive(req, "min_degree");
    int t = cJSON_IsNumber(jt) ? jt->valueint : 3;
    BTree tree;
    btree_init(&tree, t);
    cJSON* steps = cJSON_CreateArray();

    const cJSON* jins = cJSON_GetObjectItemCaseSensitive(req, "insert");
    if (cJSON_IsArray(jins)) {
        const cJSON* v = NULL;
        cJSON_ArrayForEach(v, jins) {
            if (cJSON_IsNumber(v)) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%02d", v->valueint);
                btree_insert(&tree, buf, v->valueint * 10);
                cJSON_AddItemToArray(steps, btree_structure_json(&tree));
            }
        }
    }

    size_t sz = btree_size(&tree);
    char (*keys)[MAX_CITY_NAME_LEN] = sz > 0 ? malloc(sz * MAX_CITY_NAME_LEN) : NULL;
    size_t cnt = sz > 0 ? btree_inorder_keys(&tree, keys) : 0;

    cJSON* inorder = cJSON_CreateArray();
    for (size_t i = 0; i < cnt; ++i) {
        cJSON_AddItemToArray(inorder, cJSON_CreateNumber(atoi(keys[i])));
    }
    free(keys);

    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddNumberToObject(o, "min_degree", btree_min_degree(&tree));
    cJSON_AddItemToObject(o, "steps", steps);
    cJSON_AddItemToObject(o, "final", btree_structure_json(&tree));
    cJSON_AddItemToObject(o, "inorder", inorder);
    cJSON_AddNumberToObject(o, "height", btree_height(&tree));
    cJSON_AddNumberToObject(o, "size", (double)sz);

    btree_free(&tree);
    return o;
}

static cJSON* handle_selfcheck(void) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "status", "ok");
    cJSON_AddBoolToObject(o, "avl_self_check", avl_self_check());
    cJSON_AddBoolToObject(o, "btree_self_check", btree_self_check());
    return o;
}

char* engine_handle_request(Engine* eng, const cJSON* req) {
    const cJSON* jact = cJSON_GetObjectItemCaseSensitive(req, "action");
    const char* action = (cJSON_IsString(jact) && jact->valuestring) ? jact->valuestring : "route";

    cJSON* result = NULL;
    if      (strcmp(action, "route") == 0)      result = handle_route(eng, req);
    else if (strcmp(action, "compare") == 0)    result = handle_compare(eng, req);
    else if (strcmp(action, "mst") == 0)        result = handle_mst(eng, req);
    else if (strcmp(action, "network") == 0)    result = handle_network(eng);
    else if (strcmp(action, "meta") == 0)       result = handle_meta(eng);
    else if (strcmp(action, "benchmark") == 0)  result = handle_benchmark(req);
    else if (strcmp(action, "avl_demo") == 0)   result = handle_avl_demo(req);
    else if (strcmp(action, "btree_demo") == 0) result = handle_btree_demo(req);
    else if (strcmp(action, "selfcheck") == 0)  result = handle_selfcheck();
    else {
        char err[128];
        snprintf(err, sizeof(err), "unknown action: %s", action);
        result = error_json(err);
    }

    char* dumped = cJSON_PrintUnformatted(result);
    cJSON_Delete(result);
    return dumped;
}
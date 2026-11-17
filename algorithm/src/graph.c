#include "graph.h"
#include "route_result.h"
#include "prim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void route_result_init(RouteResult* r) {
    memset(r, 0, sizeof(RouteResult));
}

void route_result_free(RouteResult* r) {
    free(r->path);
    free(r->path_edges);
    r->path           = NULL;
    r->path_len       = 0;
    r->path_edges     = NULL;
    r->path_edges_len = 0;
}

void mst_result_init(MSTResult* res) {
    memset(res, 0, sizeof(MSTResult));
}

void mst_result_free(MSTResult* res) {
    free(res->hubs);
    free(res->unreachable_hubs);
    if (res->mst_edges) {
        for (int i = 0; i < res->num_mst_edges; ++i) {
            free(res->mst_edges[i].corridor_path);
            free(res->mst_edges[i].corridor_edges);
        }
        free(res->mst_edges);
    }
    memset(res, 0, sizeof(MSTResult));
}

static void add_warning(Graph* g, const char* msg) {
    if (g->num_warnings >= g->warn_capacity) {
        int new_cap = g->warn_capacity == 0 ? 8 : g->warn_capacity * 2;
        g->warnings = (char**)realloc(g->warnings, (size_t)new_cap * sizeof(char*));
        g->warn_capacity = new_cap;
    }
    char* copy = (char*)malloc(MAX_WARN_MSG_LEN);
    if (copy) {
        strncpy(copy, msg, MAX_WARN_MSG_LEN - 1);
        copy[MAX_WARN_MSG_LEN - 1] = '\0';
        g->warnings[g->num_warnings++] = copy;
    }
}

static char* read_entire_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void edge_list_push(EdgeList* list, Edge e) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 4 : list->capacity * 2;
        list->edges = (Edge*)realloc(list->edges, (size_t)new_cap * sizeof(Edge));
        list->capacity = new_cap;
    }
    list->edges[list->count++] = e;
}

void graph_init(Graph* g) {
    memset(g, 0, sizeof(Graph));
    g->cfg.fuel_price_per_litre    = 95.0;
    g->cfg.vehicle_mileage_kmpl    = 8.0;
    g->cfg.max_speed_override_kmph = 0.0;
    g->max_speed                   = 1.0;
}

void graph_free(Graph* g) {
    free(g->cities);
    free(g->edges);
    if (g->adj) {
        for (int i = 0; i < g->num_vertices; ++i) {
            free(g->adj[i].edges);
        }
        free(g->adj);
    }
    if (g->warnings) {
        for (int i = 0; i < g->num_warnings; ++i) {
            free(g->warnings[i]);
        }
        free(g->warnings);
    }
    graph_init(g);
}

static void load_config_file(Graph* g, const char* path) {
    char* text = read_entire_file(path);
    if (!text) return;
    cJSON* doc = cJSON_Parse(text);
    free(text);
    if (!doc) return;

    const cJSON* fuel = cJSON_GetObjectItemCaseSensitive(doc, "fuel_price_per_litre");
    if (cJSON_IsNumber(fuel) && fuel->valuedouble > 0)
        g->cfg.fuel_price_per_litre = fuel->valuedouble;

    const cJSON* mil = cJSON_GetObjectItemCaseSensitive(doc, "vehicle_mileage_kmpl");
    if (cJSON_IsNumber(mil) && mil->valuedouble > 0)
        g->cfg.vehicle_mileage_kmpl = mil->valuedouble;

    const cJSON* spd = cJSON_GetObjectItemCaseSensitive(doc, "max_speed_override_kmph");
    if (cJSON_IsNumber(spd) && spd->valuedouble > 0)
        g->cfg.max_speed_override_kmph = spd->valuedouble;

    cJSON_Delete(doc);
}

static bool load_cities_file(Graph* g, const char* path, char* err_buf, size_t err_size) {
    char* text = read_entire_file(path);
    if (!text) {
        snprintf(err_buf, err_size, "cannot read %s", path);
        return false;
    }
    cJSON* doc = cJSON_Parse(text);
    free(text);
    if (!doc) {
        snprintf(err_buf, err_size, "JSON parse error in %s", path);
        return false;
    }

    const cJSON* cities = cJSON_GetObjectItemCaseSensitive(doc, "cities");
    if (!cJSON_IsArray(cities)) {
        snprintf(err_buf, err_size, "cities.json is missing a 'cities' array");
        cJSON_Delete(doc);
        return false;
    }

    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, cities) {
        if (!cJSON_IsObject(item)) {
            add_warning(g, "skipped non-object city record");
            continue;
        }
        const cJSON* jname = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON* jlat  = cJSON_GetObjectItemCaseSensitive(item, "lat");
        const cJSON* jlon  = cJSON_GetObjectItemCaseSensitive(item, "lon");

        if (!cJSON_IsString(jname) || !jname->valuestring || jname->valuestring[0] == '\0') {
            add_warning(g, "skipped city with empty name");
            continue;
        }
        double lat = cJSON_IsNumber(jlat) ? jlat->valuedouble : 1000.0;
        double lon = cJSON_IsNumber(jlon) ? jlon->valuedouble : 1000.0;
        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
            char warn[MAX_WARN_MSG_LEN];
            snprintf(warn, sizeof(warn), "skipped city '%s' (invalid coordinates)", jname->valuestring);
            add_warning(g, warn);
            continue;
        }

        int existing = -1;
        if (graph_find_city_id(g, jname->valuestring, &existing)) {
            char warn[MAX_WARN_MSG_LEN];
            snprintf(warn, sizeof(warn), "duplicate city name '%s' skipped", jname->valuestring);
            add_warning(g, warn);
            continue;
        }

        if (g->num_vertices >= g->city_capacity) {
            int new_cap = g->city_capacity == 0 ? 32 : g->city_capacity * 2;
            g->cities = (City*)realloc(g->cities, (size_t)new_cap * sizeof(City));
            g->city_capacity = new_cap;
        }

        City* c = &g->cities[g->num_vertices];
        c->id = g->num_vertices;
        strncpy(c->name, jname->valuestring, MAX_CITY_NAME_LEN - 1);
        c->name[MAX_CITY_NAME_LEN - 1] = '\0';
        c->lat = lat;
        c->lon = lon;
        g->num_vertices++;
    }

    cJSON_Delete(doc);
    if (g->num_vertices == 0) {
        snprintf(err_buf, err_size, "no valid cities were loaded");
        return false;
    }
    return true;
}

static bool load_roads_file(Graph* g, const char* path, char* err_buf, size_t err_size) {
    char* text = read_entire_file(path);
    if (!text) {
        snprintf(err_buf, err_size, "cannot read %s", path);
        return false;
    }
    cJSON* doc = cJSON_Parse(text);
    free(text);
    if (!doc) {
        snprintf(err_buf, err_size, "JSON parse error in %s", path);
        return false;
    }

    const cJSON* roads = cJSON_GetObjectItemCaseSensitive(doc, "roads");
    if (!cJSON_IsArray(roads)) {
        snprintf(err_buf, err_size, "roads.json is missing a 'roads' array");
        cJSON_Delete(doc);
        return false;
    }

    int idx = 0;
    const cJSON* item = NULL;
    cJSON_ArrayForEach(item, roads) {
        idx++;
        if (!cJSON_IsObject(item)) {
            char warn[MAX_WARN_MSG_LEN];
            snprintf(warn, sizeof(warn), "road #%d skipped (not an object)", idx);
            add_warning(g, warn);
            continue;
        }

        const cJSON* jsrc = cJSON_GetObjectItemCaseSensitive(item, "source");
        const cJSON* jdst = cJSON_GetObjectItemCaseSensitive(item, "destination");
        const char* src = cJSON_IsString(jsrc) ? jsrc->valuestring : "";
        const char* dst = cJSON_IsString(jdst) ? jdst->valuestring : "";

        int si = -1, di = -1;
        if (!graph_find_city_id(g, src, &si) || !graph_find_city_id(g, dst, &di)) {
            char warn[MAX_WARN_MSG_LEN];
            snprintf(warn, sizeof(warn), "road #%d skipped (unknown city)", idx);
            add_warning(g, warn);
            continue;
        }

        const cJSON* jdist  = cJSON_GetObjectItemCaseSensitive(item, "distance_km");
        const cJSON* jspeed = cJSON_GetObjectItemCaseSensitive(item, "avg_speed_kmph");
        const cJSON* jtoll  = cJSON_GetObjectItemCaseSensitive(item, "toll");
        const cJSON* jsf    = cJSON_GetObjectItemCaseSensitive(item, "subsidy_forward");
        const cJSON* jsr    = cJSON_GetObjectItemCaseSensitive(item, "subsidy_reverse");
        const cJSON* jhw    = cJSON_GetObjectItemCaseSensitive(item, "highway");

        double dist  = cJSON_IsNumber(jdist)  ? jdist->valuedouble  : 0.0;
        double speed = cJSON_IsNumber(jspeed) ? jspeed->valuedouble : 0.0;
        double toll  = cJSON_IsNumber(jtoll)  ? jtoll->valuedouble  : 0.0;
        double sf    = cJSON_IsNumber(jsf)    ? jsf->valuedouble    : 0.0;
        double sr    = cJSON_IsNumber(jsr)    ? jsr->valuedouble    : 0.0;

        if (dist <= 0.0 || speed <= 0.0) {
            char warn[MAX_WARN_MSG_LEN];
            snprintf(warn, sizeof(warn), "road #%d skipped (invalid distance/speed)", idx);
            add_warning(g, warn);
            continue;
        }
        if (toll < 0.0) { add_warning(g, "negative toll clamped to 0"); toll = 0.0; }
        if (sf   < 0.0) { add_warning(g, "negative subsidy_forward clamped to 0"); sf = 0.0; }
        if (sr   < 0.0) { add_warning(g, "negative subsidy_reverse clamped to 0"); sr = 0.0; }

        const char* hw = (cJSON_IsString(jhw) && jhw->valuestring) ? jhw->valuestring : "Unknown";

        if (g->num_directed_edges + 2 > g->edge_capacity) {
            int new_cap = g->edge_capacity == 0 ? 64 : g->edge_capacity * 2;
            g->edges = (Edge*)realloc(g->edges, (size_t)new_cap * sizeof(Edge));
            g->edge_capacity = new_cap;
        }

        Edge fwd = {si, di, "", dist, speed, toll, sf};
        strncpy(fwd.highway, hw, MAX_HIGHWAY_LEN - 1);

        Edge rev = {di, si, "", dist, speed, toll, sr};
        strncpy(rev.highway, hw, MAX_HIGHWAY_LEN - 1);

        g->edges[g->num_directed_edges++] = fwd;
        g->edges[g->num_directed_edges++] = rev;
    }

    cJSON_Delete(doc);
    if (g->num_directed_edges == 0) {
        snprintf(err_buf, err_size, "no valid roads were loaded");
        return false;
    }
    return true;
}

bool graph_load(Graph* g, const char* data_dir, char* err_buf, size_t err_size) {
    graph_free(g);

    char path[MAX_PATH_LEN];
    size_t len = strlen(data_dir);
    bool has_slash = (len > 0 && (data_dir[len - 1] == '/' || data_dir[len - 1] == '\\'));
    const char* sep = has_slash ? "" : "/";

    snprintf(path, sizeof(path), "%s%sconfig.json", data_dir, sep);
    load_config_file(g, path);

    snprintf(path, sizeof(path), "%s%scities.json", data_dir, sep);
    if (!load_cities_file(g, path, err_buf, err_size)) return false;

    snprintf(path, sizeof(path), "%s%sroads.json", data_dir, sep);
    if (!load_roads_file(g, path, err_buf, err_size)) return false;

    g->adj = (EdgeList*)calloc((size_t)g->num_vertices, sizeof(EdgeList));
    for (int i = 0; i < g->num_directed_edges; ++i) {
        edge_list_push(&g->adj[g->edges[i].from], g->edges[i]);
    }

    g->max_speed = g->cfg.max_speed_override_kmph;
    for (int i = 0; i < g->num_directed_edges; ++i) {
        if (g->edges[i].avg_speed_kmph > g->max_speed) {
            g->max_speed = g->edges[i].avg_speed_kmph;
        }
    }
    if (g->max_speed <= 0.0) g->max_speed = 1.0;

    return true;
}

bool graph_find_city_id(const Graph* g, const char* name, int* out_id) {
    if (!name) return false;
    for (int i = 0; i < g->num_vertices; ++i) {
        if (strcmp(g->cities[i].name, name) == 0) {
            *out_id = g->cities[i].id;
            return true;
        }
    }
    return false;
}

double graph_fuel_cost(const Graph* g, double distance_km) {
    if (g->cfg.vehicle_mileage_kmpl <= 0.0) return 0.0;
    return (distance_km / g->cfg.vehicle_mileage_kmpl) * g->cfg.fuel_price_per_litre;
}

double graph_financial_cost(const Graph* g, const Edge* e) {
    return e->toll + graph_fuel_cost(g, e->distance_km) - e->subsidy;
}

double graph_edge_weight(const Graph* g, const Edge* e, WeightMode mode) {
    switch (mode) {
        case WEIGHT_TIME:      return edge_time_hours(e);
        case WEIGHT_DISTANCE:  return e->distance_km;
        case WEIGHT_FINANCIAL: return graph_financial_cost(g, e);
    }
    return e->distance_km;
}

bool graph_has_negative_financial_edge(const Graph* g) {
    for (int i = 0; i < g->num_directed_edges; ++i) {
        if (graph_financial_cost(g, &g->edges[i]) < 0.0) return true;
    }
    return false;
}

cJSON* graph_to_summary_json(const Graph* g) {
    cJSON* o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "num_cities", g->num_vertices);
    cJSON_AddNumberToObject(o, "num_roads", graph_num_undirected_roads(g));
    cJSON_AddNumberToObject(o, "num_directed_edges", g->num_directed_edges);
    cJSON_AddBoolToObject(o, "has_negative_financial_edge", graph_has_negative_financial_edge(g));
    cJSON_AddNumberToObject(o, "fuel_price_per_litre", g->cfg.fuel_price_per_litre);
    cJSON_AddNumberToObject(o, "vehicle_mileage_kmpl", g->cfg.vehicle_mileage_kmpl);
    cJSON_AddNumberToObject(o, "max_speed_kmph", g->max_speed);

    cJSON* citiesArr = cJSON_CreateArray();
    for (int i = 0; i < g->num_vertices; ++i) {
        cJSON* cj = cJSON_CreateObject();
        cJSON_AddNumberToObject(cj, "id", g->cities[i].id);
        cJSON_AddStringToObject(cj, "name", g->cities[i].name);
        cJSON_AddNumberToObject(cj, "lat", g->cities[i].lat);
        cJSON_AddNumberToObject(cj, "lon", g->cities[i].lon);
        cJSON_AddItemToArray(citiesArr, cj);
    }
    cJSON_AddItemToObject(o, "cities", citiesArr);

    cJSON* roadsArr = cJSON_CreateArray();
    for (int i = 0; i < g->num_directed_edges; ++i) {
        const Edge* e = &g->edges[i];
        if (e->from > e->to) continue;
        cJSON* rj = cJSON_CreateObject();
        cJSON_AddStringToObject(rj, "source", g->cities[e->from].name);
        cJSON_AddStringToObject(rj, "destination", g->cities[e->to].name);
        cJSON_AddStringToObject(rj, "highway", e->highway);
        cJSON_AddNumberToObject(rj, "distance_km", e->distance_km);
        cJSON_AddNumberToObject(rj, "avg_speed_kmph", e->avg_speed_kmph);
        cJSON_AddNumberToObject(rj, "toll", e->toll);
        cJSON_AddItemToArray(roadsArr, rj);
    }
    cJSON_AddItemToObject(o, "roads", roadsArr);

    cJSON* warnArr = cJSON_CreateArray();
    for (int i = 0; i < g->num_warnings; ++i) {
        cJSON_AddItemToArray(warnArr, cJSON_CreateString(g->warnings[i]));
    }
    cJSON_AddItemToObject(o, "warnings", warnArr);

    return o;
}
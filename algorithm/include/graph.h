#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

#define MAX_CITY_NAME_LEN 64
#define MAX_HIGHWAY_LEN   32
#define MAX_WARN_MSG_LEN  256
#define MAX_PATH_LEN      512

typedef enum {
    WEIGHT_TIME = 0,
    WEIGHT_DISTANCE,
    WEIGHT_FINANCIAL
} WeightMode;

typedef struct {
    int    id;
    char   name[MAX_CITY_NAME_LEN];
    double lat;
    double lon;
} City;

typedef struct {
    int    from;
    int    to;
    char   highway[MAX_HIGHWAY_LEN];
    double distance_km;
    double avg_speed_kmph;
    double toll;
    double subsidy; /* applies in THIS direction only */
} Edge;

typedef struct {
    Edge* edges;
    int   count;
    int   capacity;
} EdgeList;

typedef struct {
    double fuel_price_per_litre;    /* default: 95.0 */
    double vehicle_mileage_kmpl;    /* default: 8.0  */
    double max_speed_override_kmph; /* 0.0 => derive from dataset */
} FinancialConfig;

typedef struct {
    City*           cities;
    int             num_vertices;
    int             city_capacity;

    EdgeList*       adj;            /* array of EdgeList, size = num_vertices */
    Edge*           edges;          /* flat list of all directed edges */
    int             num_directed_edges;
    int             edge_capacity;

    FinancialConfig cfg;
    double          max_speed;

    char**          warnings;
    int             num_warnings;
    int             warn_capacity;
} Graph;

/* Lifecycle */
void graph_init(Graph* g);
void graph_free(Graph* g);

/* Dataset loading (cities.json, roads.json, optional config.json) */
bool graph_load(Graph* g, const char* data_dir, char* err_buf, size_t err_size);

/* Inline helpers & accessors */
static inline double edge_time_hours(const Edge* e) {
    return (e->avg_speed_kmph > 0.0) ? (e->distance_km / e->avg_speed_kmph) : 0.0;
}

static inline int graph_num_undirected_roads(const Graph* g) {
    return g->num_directed_edges / 2;
}

bool   graph_find_city_id(const Graph* g, const char* name, int* out_id);
double graph_fuel_cost(const Graph* g, double distance_km);
double graph_financial_cost(const Graph* g, const Edge* e); /* toll + fuel - subsidy */
double graph_edge_weight(const Graph* g, const Edge* e, WeightMode mode);
bool   graph_has_negative_financial_edge(const Graph* g);

/* Network overview used by the dashboard /api/network endpoint */
cJSON* graph_to_summary_json(const Graph* g);

#endif /* GRAPH_H */
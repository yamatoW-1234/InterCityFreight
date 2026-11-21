#ifndef GRAPH_H
#define GRAPH_H

#include "vehicle.h"

#define MAX_CITIES 128
#define MAX_HIGHWAY_LEN 64

typedef struct {
    int id;
    char name[64];
    char state[48];
    double lat;
    double lon;
} City;

typedef struct Edge {
    int from;
    int to;
    char highway[MAX_HIGHWAY_LEN];
    double distance_km;
    double speed_limit_kmh;
    double toll_inr;
    double subsidy_inr;
    double traffic_multiplier; /* 1.0 = free flow, >1.0 = slower/congested */
    int open_hour;             /* 0..24 */
    int close_hour;            /* 0..24 */
    int is_blocked;            /* 1 = simulated roadblock */
    struct Edge *next;
} Edge;

typedef struct {
    int num_cities;
    City cities[MAX_CITIES];
    Edge *adj[MAX_CITIES];
    int avoid_city[MAX_CITIES];
} Graph;

void graph_init(Graph *g);
void graph_free(Graph *g);
void graph_add_city(Graph *g, int id, const char *name, const char *state, double lat, double lon);
void graph_add_edge(Graph *g, const Edge *e);
double haversine_km(double lat1, double lon1, double lat2, double lon2);
double edge_effective_speed(const Edge *e, const Vehicle *v);
double edge_travel_hours(const Edge *e, const Vehicle *v);
double edge_net_cost_inr(const Edge *e, const Vehicle *v, double fuel_price_inr);
int edge_is_open_at(const Edge *e, double hour_of_day, double *wait_hours_out);

#endif

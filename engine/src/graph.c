#include "graph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void graph_init(Graph *g) {
    memset(g, 0, sizeof(Graph));
    for (int i = 0; i < MAX_CITIES; i++) {
        g->cities[i].id = -1;
        g->adj[i] = NULL;
        g->avoid_city[i] = 0;
    }
}

void graph_free(Graph *g) {
    for (int i = 0; i < MAX_CITIES; i++) {
        Edge *curr = g->adj[i];
        while (curr) {
            Edge *nxt = curr->next;
            free(curr);
            curr = nxt;
        }
        g->adj[i] = NULL;
    }
}

void graph_add_city(Graph *g, int id, const char *name, const char *state, double lat, double lon) {
    if (id < 0 || id >= MAX_CITIES) return;
    g->cities[id].id = id;
    strncpy(g->cities[id].name, name ? name : "", sizeof(g->cities[id].name) - 1);
    strncpy(g->cities[id].state, state ? state : "", sizeof(g->cities[id].state) - 1);
    g->cities[id].lat = lat;
    g->cities[id].lon = lon;
    if (id + 1 > g->num_cities) {
        g->num_cities = id + 1;
    }
}

void graph_add_edge(Graph *g, const Edge *e) {
    if (!e || e->from < 0 || e->from >= MAX_CITIES || e->to < 0 || e->to >= MAX_CITIES) return;
    Edge *node = (Edge *)malloc(sizeof(Edge));
    if (!node) return;
    *node = *e;
    if (node->traffic_multiplier <= 0.0) node->traffic_multiplier = 1.0;
    if (node->close_hour <= 0 && node->open_hour == 0) node->close_hour = 24;
    node->next = g->adj[e->from];
    g->adj[e->from] = node;
}

double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    double r = 6371.0;
    double p1 = lat1 * M_PI / 180.0;
    double p2 = lat2 * M_PI / 180.0;
    double dp = (lat2 - lat1) * M_PI / 180.0;
    double dl = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dp / 2.0) * sin(dp / 2.0) +
               cos(p1) * cos(p2) * sin(dl / 2.0) * sin(dl / 2.0);
    return 2.0 * r * atan2(sqrt(a), sqrt(1.0 - a));
}

double edge_effective_speed(const Edge *e, const Vehicle *v) {
    double base_speed = e->speed_limit_kmh > 0.0 ? e->speed_limit_kmh : 65.0;
    if (v && v->max_speed_kmh > 0.0 && v->max_speed_kmh < base_speed) {
        base_speed = v->max_speed_kmh;
    }
    double mult = e->traffic_multiplier > 0.1 ? e->traffic_multiplier : 1.0;
    return base_speed / mult;
}

double edge_travel_hours(const Edge *e, const Vehicle *v) {
    double spd = edge_effective_speed(e, v);
    if (spd <= 1.0) spd = 1.0;
    return e->distance_km / spd;
}

double edge_net_cost_inr(const Edge *e, const Vehicle *v, double fuel_price_inr) {
    double mileage = (v && v->mileage_kmpl > 0.1) ? v->mileage_kmpl : 3.5;
    double toll_mult = (v && v->toll_multiplier > 0.1) ? v->toll_multiplier : 1.0;
    double fuel_cost = (e->distance_km / mileage) * fuel_price_inr;
    double toll_cost = e->toll_inr * toll_mult;
    return fuel_cost + toll_cost - e->subsidy_inr;
}

int edge_is_open_at(const Edge *e, double hour_of_day, double *wait_hours_out) {
    if (wait_hours_out) *wait_hours_out = 0.0;
    int open_h = e->open_hour;
    int close_h = e->close_hour;
    if (open_h == 0 && close_h == 24) return 1;

    double tod = fmod(hour_of_day, 24.0);
    if (tod < 0.0) tod += 24.0;

    if (open_h < close_h) {
        if (tod >= open_h && tod < close_h) return 1;
        if (wait_hours_out) {
            *wait_hours_out = (tod < open_h) ? (open_h - tod) : (24.0 - tod + open_h);
        }
        return 0;
    } else {
        /* Overnight window (e.g., 22 to 6) */
        if (tod >= open_h || tod < close_h) return 1;
        if (wait_hours_out) {
            *wait_hours_out = open_h - tod;
        }
        return 0;
    }
}

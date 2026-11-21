#ifndef DP_KNAPSACK_H
#define DP_KNAPSACK_H

#include "algorithms.h"

#define MAX_STOPS 12
#define MAX_CARGO_ITEMS 64

typedef struct {
    int city_id;
    char item_name[64];
    char action[16]; /* "pickup" or "dropoff" */
    double weight_kg;
    double value_inr;
    double taken_kg;
    double fraction;
    double allocated_value_inr;
} CargoItem;

typedef struct {
    int item_count;
    CargoItem items[MAX_CARGO_ITEMS];
    double capacity_kg;
    double used_kg;
    double total_value_inr;
} KnapsackResult;

/* Held-Karp Bitmask DP for optimal ordering of mandatory stops + itinerary stitching */
PathResult run_dp_stop_planner(const Graph *g, const Vehicle *v,
                               int origin, int destination,
                               const int *stops, int num_stops,
                               double departure_hr, double fuel_price_inr,
                               const char *optimize_for, int respect_windows);

/* Greedy Fractional/Partial Knapsack for warehouse cargo allocation */
KnapsackResult run_partial_knapsack(CargoItem *items, int count, double capacity_kg);

#endif

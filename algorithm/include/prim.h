#ifndef PRIM_H
#define PRIM_H

#include <stdbool.h>
#include "route_result.h"

typedef struct {
    int    from_hub;          /* city id of first hub */
    int    to_hub;            /* city id of second hub */
    double distance_km;       /* shortest-path corridor distance */
    double cost;              /* financial cost along the corridor */

    int*   corridor_path;     /* actual city sequence of the corridor */
    int    corridor_path_len;

    Edge*  corridor_edges;
    int    corridor_edges_len;
} MSTEdgeResult;

typedef struct {
    bool           success;
    char           status_message[MAX_STATUS_LEN];

    int*           hubs;               /* selected hub city ids */
    int            num_hubs;

    MSTEdgeResult* mst_edges;
    int            num_mst_edges;

    double         total_distance_km;
    double         total_cost;
    double         execution_time_ms;

    bool           disconnected;
    int*           unreachable_hubs;   /* city ids not reachable by the MST */
    int            num_unreachable_hubs;
} MSTResult;

/* Lifecycle */
void      mst_result_init(MSTResult* res);
void      mst_result_free(MSTResult* res);

/* Prim's Minimum Spanning Tree on the metric closure of selected hubs */
MSTResult freight_prim_mst(const Graph* g, const int* hubs, int num_hubs);

#endif /* PRIM_H */
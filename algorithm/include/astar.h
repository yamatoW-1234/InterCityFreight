#ifndef ASTAR_H
#define ASTAR_H

#include "route_result.h"

/* A* search for the fastest freight-transit route.
 * Edge weight = estimated travel time (hours).
 * Heuristic = Haversine straight-line distance / max_speed_kmph. */
RouteResult freight_astar(const Graph* g, int source, int dest);

#endif /* ASTAR_H */
#ifndef BELLMAN_FORD_H
#define BELLMAN_FORD_H

#include "route_result.h"

/* Bellman-Ford shortest path for the financial objective (toll + fuel - subsidy).
 * Supports negative edges and detects reachable negative-weight cycles. */
RouteResult freight_bellman_ford(const Graph* g, int source, int dest);

#endif /* BELLMAN_FORD_H */
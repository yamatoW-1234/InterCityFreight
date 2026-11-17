#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "route_result.h"

/* Dijkstra's shortest-path algorithm (non-negative weights).
 * Backed by the AVL-tree indexed min-priority queue (avl_pq.h). */
RouteResult freight_dijkstra(const Graph* g, int source, int dest, WeightMode mode);

#endif /* DIJKSTRA_H */
#include <stdio.h>
#include "algorithms.h"
#include "trees.h"
#include <string.h>

MSTResult run_prims_mst(const Graph *g, const int *hubs, int num_hubs) {
    MSTResult res;
    memset(&res, 0, sizeof(MSTResult));
    if (!hubs || num_hubs < 2) return res;

    /* Compute pairwise shortest-path distance between all selected hubs */
    double pair_dist[MAX_CITIES][MAX_CITIES];
    char pair_hw[MAX_CITIES][MAX_CITIES][MAX_HIGHWAY_LEN];
    Vehicle dummy = {.mileage_kmpl = 3.5, .max_speed_kmh = 80.0, .toll_multiplier = 1.0};

    for (int i = 0; i < num_hubs; i++) {
        for (int j = i + 1; j < num_hubs; j++) {
            int u = hubs[i];
            int v = hubs[j];
            PathResult p = run_dijkstra(g, &dummy, u, v, 0.0, 95.0, 0);
            if (p.found) {
                pair_dist[i][j] = pair_dist[j][i] = p.total_distance_km;
                snprintf(pair_hw[i][j], MAX_HIGHWAY_LEN, "%s",
                         p.path_len > 1 ? p.highways[0] : "Highway");
                snprintf(pair_hw[j][i], MAX_HIGHWAY_LEN, "%s", pair_hw[i][j]);
            } else {
                pair_dist[i][j] = pair_dist[j][i] = INF_COST;
                pair_hw[i][j][0] = pair_hw[j][i][0] = '\0';
            }
        }
    }

    int in_mst[MAX_CITIES] = {0};
    double key[MAX_CITIES];
    int parent_idx[MAX_CITIES];

    for (int i = 0; i < num_hubs; i++) {
        key[i] = INF_COST;
        parent_idx[i] = -1;
    }

    AVLPriorityQueue pq;
    avl_pq_init(&pq);

    key[0] = 0.0;
    avl_pq_push(&pq, 0.0, 0, 0.0);

    int visited_count = 0;
    while (!avl_pq_empty(&pq)) {
        double k_u, aux;
        int u_idx;
        if (!avl_pq_pop_min(&pq, &k_u, &u_idx, &aux)) break;
        (void)k_u;
        (void)aux;
        if (in_mst[u_idx]) continue;
        in_mst[u_idx] = 1;
        visited_count++;

        if (parent_idx[u_idx] != -1) {
            int p_idx = parent_idx[u_idx];
            MSTEdge *me = &res.edges[res.edge_count++];
            me->from = hubs[p_idx];
            me->to = hubs[u_idx];
            me->distance_km = pair_dist[p_idx][u_idx];
            strncpy(me->highway, pair_hw[p_idx][u_idx], MAX_HIGHWAY_LEN - 1);
            res.total_distance_km += me->distance_km;
        }

        for (int v_idx = 0; v_idx < num_hubs; v_idx++) {
            if (!in_mst[v_idx] && pair_dist[u_idx][v_idx] < key[v_idx]) {
                key[v_idx] = pair_dist[u_idx][v_idx];
                parent_idx[v_idx] = u_idx;
                avl_pq_push(&pq, key[v_idx], v_idx, 0.0);
            }
        }
    }

    avl_pq_free(&pq);
    res.connected = (visited_count == num_hubs);
    return res;
}

# Algorithms

All four algorithms are implemented from scratch in C++ (`algorithm_engine/src/`).

## A* Search — fastest transit (`astar.cpp`)
- Edge weight = estimated travel time `distance / avg_speed`.
- Heuristic `h(n) = haversine(n, goal) / max_speed_in_dataset`.
- Admissibility: straight-line distance ≤ any road distance and
  `max_speed ≥` every edge speed, so `h(n) ≤` true remaining time.
- Priority queue: AVL-backed indexed min-PQ keyed by `f = g + h`.
- Maintains open/closed sets, reconstructs path, records expanded vertices.

## Dijkstra — shortest distance / non-negative cost (`dijkstra.cpp`)
- Greedy best-first expansion using the AVL indexed min-PQ.
- Distance mode for `shortest`; Financial mode only when all net costs ≥ 0.
- Explicitly **rejects** graphs containing negative financial edges.

## Bellman-Ford — minimum financial cost (`bellman_ford.cpp`)
- Weight = `toll + fuel − subsidy` (can be negative via subsidies).
- Relaxes all edges up to `V−1` times (early exit when stable).
- Detects reachable negative cycles by one extra pass + forward propagation;
  reports them as an error (cost unbounded), never as a "saving".
- Handles unreachable destinations and reconstructs the path.

## Prim's MST — multi-hub infrastructure (`prim.cpp`)
- Runs on the **metric closure** of the selected hubs: the weight between two
  hubs is their Dijkstra shortest-path road distance.
- Greedy nearest-hub attachment with the AVL indexed min-PQ.
- Detects disconnected hub sets and reports unreachable hubs.
- Returns each MST edge with its real road corridor (path, distance, cost).
- This is infrastructure planning, not a point-to-point delivery route.

## Supporting data structures
- **AVL Tree** (`avl_tree.cpp`): insert/delete/search, height, balance factor,
  LL/RR/LR/RL rotations, three traversals. Backs the priority queue.
- **AVL indexed min-PQ** (`avl_pq.cpp`): `(priority, sequence)` keys keep
  duplicate priorities unique; `index_` map enables O(log n) decrease-key.
- **B-Tree** (`b_tree.cpp`): configurable minimum degree `t`, proactive node
  splitting on insert, search, in-order traversal. Indexes `city_name → id`.
# Complexity Analysis

Theoretical complexity (below) is **separate** from the experimental benchmark
timings produced by the Algorithm Analysis page (`/api/benchmark`).

| Algorithm     | Best             | Average          | Worst            | Space |
|---------------|------------------|------------------|------------------|-------|
| A* Search       | O(E)             | O((V+E) log V)   | O((V+E) log V)   | O(V)  |
| Dijkstra        | O((V+E) log V)   | O((V+E) log V)   | O((V+E) log V)   | O(V)  |
| Bellman-Ford    | O(V·E)           | O(V·E)           | O(V·E)           | O(V)  |
| Prim (binary/AVL PQ) | O((V+E) log V) | O((V+E) log V) | O((V+E) log V)   | O(V)  |
| AVL insert/delete/search | O(log V) | O(log V)      | O(log V)         | O(V)  |
| B-Tree insert/search     | O(log V) | O(log V)      | O(log V)         | O(V)  |

Notes:
- `V` = number of cities (vertices), `E` = number of road edges.
- The `log V` factor comes from the AVL-backed priority queue operations.
- Bellman-Ford is asymptotically slower but is the only correct choice when
  negative net-cost edges (subsidies) are present.
- A*'s practical expansion count is usually far below Dijkstra's thanks to the
  admissible heuristic; this is visible in the "expanded vertices" metric.

## Experimental benchmark
`handle_benchmark` builds synthetic graphs of sizes `[10, 25, 50, 100, 200, 400]`
with a reproducible PRNG, runs all four algorithms, and returns measured
`execution_time_ms` for each. Plot these to contrast empirical growth with the
theoretical orders above.
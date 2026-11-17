# Inter-City Freight Logistics & Route Generator

An intelligent decision-support system that models major Indian cities and their
highways as a weighted graph and automatically selects the correct graph
algorithm for each freight objective:

| Objective            | Algorithm selected automatically |
|----------------------|----------------------------------|
| Fastest transit      | **A\* Search** (admissible Haversine time heuristic) |
| Minimum financial cost (negative adjustments possible) | **Bellman-Ford** |
| Minimum cost, all non-negative | **Dijkstra** |
| Shortest distance    | **Dijkstra** |
| Multi-hub infrastructure backbone | **Prim's MST** (metric closure over hubs) |

Every route result includes total distance (km), estimated transit time (hours),
toll (₹), fuel (₹), subsidy (₹), total cost (₹), engine execution time (ms) and
expanded-vertex counts.

---

## Technology stack

- **C++17** algorithmic engine (from-scratch A*, Dijkstra, Bellman-Ford, Prim,
  AVL tree, AVL-backed indexed priority queue, B-Tree) built with **CMake**/Clang.
- **Python 3 + Flask** backend acting strictly as a subprocess bridge (JSON in/out).
- **HTML5 / CSS3 / JavaScript** frontend with **Leaflet.js + OpenStreetMap** and
  **Chart.js**.
- Dataset stored as **JSON** in `data/` (simulated, clearly labelled values).

---

## Directory structure

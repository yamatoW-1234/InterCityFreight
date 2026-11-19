# Inter-City Freight Logistics & Route Generator

An intelligent decision-support system that models major Indian commercial freight hubs and highway corridors as a weighted graph and automatically selects the optimal graph algorithm for each logistics objective:

| Freight Objective | Algorithm Selected Automatically |
| :--- | :--- |
| **Fastest transit** | **A\* Search** (admissible Haversine time heuristic) |
| **Minimum financial cost** *(negative subsidy adjustments possible)* | **Bellman-Ford** |
| **Minimum financial cost** *(all non-negative weights)* | **Dijkstra** |
| **Shortest distance** | **Dijkstra** |
| **Multi-hub infrastructure backbone** | **Prim's MST** (metric closure over hubs) |

Every route result outputs total distance (km), estimated transit time (hours), toll (₹), fuel cost (₹), corridor subsidy (₹), net freight cost (₹), engine execution time (ms), and expanded-vertex counts.

---

## Technology Stack

- **C (C11/C17)** algorithmic engine (`algorithm/`): from-scratch implementations of A\*, Dijkstra, Bellman-Ford, Prim's MST, AVL Tree, AVL-backed Indexed Priority Queue, B-Tree, Haversine distance calculations, and `cJSON` parsing, compiled via `Makefile`.
- **Node.js (Express.js)** backend (`server/`): acts as a REST API and subprocess bridge (`JSON` in/out) to the compiled C routing binary.
- **HTML5 / CSS3 / JavaScript** frontend (`public/`): interactive corridor map powered by **Leaflet.js + OpenStreetMap** alongside cost, time, and algorithm performance visualizations using **Chart.js**.
- **Dataset & Preprocessing** (`data/`, `scripts/build_data.py`): structured `cities.json`, `roads.json`, and `config.json` defining the 24 Indian commercial hubs and highway network topology.

---

## Directory Structure

```text
InterCityFreight/
├── algorithm/                  # C routing engine & custom data structures
│   ├── Makefile                # Build configuration for the C binary
│   ├── include/                # Header files (astar, dijkstra, bellman_ford, prim, avl_tree, avl_pq, b_tree, graph, engine, haversine, cJSON)
│   └── src/                    # C source implementations & CLI entry point (main.c)
├── data/                       # Graph and hub datasets
│   ├── cities.json             # 24 Indian commercial freight hubs (WGS-84 coordinates)
│   ├── roads.json              # Inter-city highway & expressway segments
│   └── config.json             # Engine & cost model parameters
├── public/                     # Static web frontend
│   ├── index.html              # Main application shell
│   ├── css/styles.css          # UI styling
│   └── js/                     # Map rendering (map.js), route planner (planner.js, route.js), comparisons (compare.js), and analytics (analysis.js, charts.js, dashboard.js, visualizer.js)
├── scripts/                    # Build, data generation, and launch helpers
│   ├── build.sh                # Compiles C engine and installs Node dependencies
│   ├── build_data.py           # Generates/processes city and road graph JSON data
│   └── run.sh                  # Starts the Express server
├── server/                     # Node.js backend & C engine bridge
│   ├── server.js               # Express server entry point
│   ├── config.js               # Server & binary path configuration
│   ├── engineBridge.js         # Spawns C engine subprocess and parses JSON output
│   ├── routes/apiRoutes.js     # REST endpoints for routing, MST, and analytics
│   └── services/analytics.js   # Route metrics aggregation
├── tests/                      # Automated test suites
│   ├── run_tests.sh            # Master test runner
│   ├── test_engine.py          # Python verification suite for C engine outputs
│   ├── c/test_engine.c         # C unit tests
│   └── integration/api.test.js # API integration tests
├── package.json                # Node.js dependencies and scripts
└── readme.md                   # Project documentation
```

---

## Getting Started

### Prerequisites
- **GCC / Clang** and **Make**
- **Node.js** (v18+) and **npm**
- **Python 3** (optional, for regenerating datasets via `scripts/build_data.py` or running `tests/test_engine.py`)

### Build & Run
1. **Build the C engine and install Node.js dependencies:**
   ```bash
   chmod +x scripts/build.sh scripts/run.sh tests/run_tests.sh
   ./scripts/build.sh
   ```
   *(Or manually run `make -C algorithm` and `npm install`.)*

2. **Start the web server:**
   ```bash
   ./scripts/run.sh
   ```
   *(Or run `npm start` / `node server/server.js`.)*

3. **Open the dashboard:**
   Navigate to `http://localhost:3000` in your browser to plan routes, inspect multi-hub MST backbones, and compare algorithm performance.

### Running Tests
```bash
./tests/run_tests.sh
```
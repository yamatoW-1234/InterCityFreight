# Cargo-Compass — Inter-City Freight Routing Engine

An intelligent inter-city freight logistics and route optimization system built for **Design and Analysis of Algorithms (DAA)**. **Cargo-Compass** models India's National Highway and expressway network as a weighted directed graph, dynamically selects the optimal algorithm pipeline in **C**, orchestrates live traffic and data services in **Node.js**, and renders an interactive monochrome map dashboard in **HTML/CSS/JS**.

---

## Key Features

* **Automated Decision Controller (C Engine)**: Evaluates user constraints and invokes the best-suited algorithm pipeline (`A*`, `Bellman-Ford`, `Dijkstra`, `Prim's MST`, or `Held-Karp DP`).
* **Real-Time Traffic & Simulated Roadblocks**: Background 15-second congestion simulator (`trafficSim.js`) plus click-to-toggle roadblocks directly on map edges.
* **Mandatory Stops & Restrictions**: Set required waypoints (ordered optimally via **Held-Karp Bitmask DP**) and mark cities to avoid.
* **Time-Windowed Itinerary & Driver Stoppages**: Respects highway `[open_hour, close_hour]` windows and injects driver **Rest & Food (+0.5h)** and **Sleep & Truck Parking (+8.0h)** breaks with nearby parking, dhaba/restaurant, and repair facilities (`stoppages.json`).
* **Dynamic Cargo Pickup & Reroute (Partial Knapsack)**: Truck discovers warehouse cargos (`cargos.json`, 300 items across 30 hubs) only upon arriving at a stop, fills remaining payload via **Partial/Fractional Knapsack**, and dynamically replans its route to include drop-off destinations.
* **Fleet & Custom Truck Builder**: 11 real Indian commercial trucks (`vehicles.json`) plus a custom builder that auto-derives axle class, NHAI toll multiplier (`1.0x`–`1.9x`), horsepower, and mileage.
* **Interactive Monochrome UI**: **Midnight Monochrome** (CARTO Dark Matter) and **Afternoon Monochrome** (CARTO Positron) themes, fullscreen map mode, and persistent **Saved Plans** (`saved_plans.json`).

---

## Algorithms & Data Structures

| Component | Purpose | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- |
| **A\* Search** (`astar.c`) | Fastest transit using Haversine GPS heuristic | $O((V + E) \log V)$ | $O(V)$ |
| **Bellman-Ford** (`bellman_ford.c`) | Cost & state-rebate optimization (supports negative net weights) | $O(V \times E)$ | $O(V)$ |
| **Dijkstra's Algorithm** (`dijkstra.c`) | Shortest non-negative distance path | $O((V + E) \log V)$ | $O(V)$ |
| **Prim's MST** (`prims.c`) | Minimum-cost inter-hub infrastructure backbone | $O((V + E) \log V)$ | $O(V)$ |
| **Held-Karp Bitmask DP** (`dp_planner.c`) | Optimal ordering across $K$ mandatory stops | $O(K^2 \cdot 2^K)$ | $O(K \cdot 2^K)$ |
| **Partial Knapsack** (`knapsack.c`) | Greedy value-per-kg cargo loading at each stop | $O(N \log N)$ | $O(N)$ |
| **AVL Tree** (`avl_tree.c`) | Self-balancing priority queue for A\*, Dijkstra, and Prim's | $O(\log V)$ per op | $O(V)$ |
| **B-Tree** (`b_tree.c`) | Spatial/city metadata lookup index | $O(\log V)$ per lookup | $O(V)$ |

---

## Repository Structure

```text
intercityfreight/
├── Makefile                        # Builds bin/freight_engine
├── package.json                    # Node.js dependencies & scripts
├── .gitignore
├── engine/
│   ├── include/
│   │   ├── algorithms.h
│   │   ├── controller.h
│   │   ├── dp_knapsack.h
│   │   ├── graph.h
│   │   ├── trees.h
│   │   └── vehicle.h
│   └── src/
│       ├── astar.c
│       ├── avl_tree.c
│       ├── b_tree.c
│       ├── bellman_ford.c
│       ├── controller.c
│       ├── dijkstra.c
│       ├── dp_planner.c
│       ├── graph.c
│       ├── knapsack.c
│       ├── main.c
│       └── prims.c
├── data/
│   ├── cities.json                 # 30 Indian logistics hubs
│   ├── roads.json                  # 136 directed highway edges
│   ├── vehicles.json               # 11 commercial truck profiles
│   ├── cargos.json                 # 300 warehouse cargo items
│   ├── stoppages.json              # City & mid-highway parking/food/repair stops
│   ├── saved_plans.json            # Persisted routes & MST hub plans
│   ├── fetch_data.py               # Geocoding, OSRM, HERE/TollGuru data fetcher
│   └── fetch_stoppages.py          # HERE Discover / Overpass stoppage fetcher
├── server/
│   ├── server.js                   # Express entry point & .env loader
│   ├── routes/
│   │   ├── routeApi.js             # /api/cities, /roads, /optimize, /plans
│   │   ├── trafficApi.js           # /api/traffic, /tick, /roadblock, /reset
│   │   ├── vehicleApi.js           # /api/vehicles
│   │   └── cargoApi.js             # /api/cargo
│   └── services/
│       ├── cBridge.js              # Spawns compiled C engine over stdin/stdout JSON
│       ├── trafficSim.js           # Simulated congestion & roadblock manager
│       ├── stoppagePlanner.js      # Food/sleep breaks & facility enrichment
│       └── planStore.js            # Saved plans persistence
└── public/
    ├── index.html
    ├── css/
    │   ├── theme.css               # Midnight & Afternoon Monochrome themes
    │   └── components.css          # Controls, timeline, modal, fullscreen map
    └── js/
        ├── app.js
        ├── mapView.js
        ├── stopManager.js
        ├── vehiclePlanner.js
        ├── cargoPanel.js
        ├── itineraryPlanner.js
        └── historyMenu.js
```

---

## Setup & Running Locally

### Prerequisites
* **Node.js** v20.12+ (uses built-in `process.loadEnvFile()`)
* **GCC** (MinGW-w64 on Windows, or GCC/Clang on Linux/macOS)
* **Python 3** (optional, only if re-fetching datasets)

### 1. Clone & Install Dependencies
```bash
git clone https://github.com/yamatow-1234/intercityfreight.git
cd intercityfreight
npm install
```

### 2. Configure Environment Variables
Create a `.env` file in the project root:
```env
PORT=3000
CARTO_API_KEY=your_carto_api_key
HERE_API_KEY=your_here_api_key
TOLLGURU_API_KEY=your_tollguru_api_key
```

### 3. Compile the C Engine
**Linux / macOS:**
```bash
make
```

**Windows (Git Bash / MinGW):**
```bash
mkdir -p bin
gcc -std=c11 -O2 -Iengine/include engine/src/*.c -o bin/freight_engine.exe -lm
```

### 4. Start the Server
```bash
npm start
```
Open **http://localhost:3000** in your browser.

---

## API Endpoints

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/config` | Returns runtime map configuration (`CARTO_API_KEY`) |
| `GET` | `/api/cities` | Lists all 30 Indian hub cities |
| `GET` | `/api/roads` | Lists highway edges merged with live traffic & roadblocks |
| `POST` | `/api/optimize` | Runs the compiled C engine (`A*`, `Bellman-Ford`, `Dijkstra`, `Prim's`, `DP`, `Knapsack`) |
| `GET` / `POST` / `DELETE` | `/api/plans` | Manage saved routes and MST hub plans |
| `GET` / `POST` | `/api/traffic`, `/tick`, `/roadblock`, `/reset` | Inspect or mutate simulated traffic & roadblocks |
| `GET` / `POST` / `DELETE` | `/api/vehicles` | List fleet trucks or create auto-derived custom trucks |
| `GET` / `POST` / `DELETE` | `/api/cargo` | Manage warehouse cargo items for Partial Knapsack |

---

## Contributors

* **Kushagra Karunya Singh** (`251210056`)
* **Krishna Gautam** (`251210054`)
* **Kunga Tsewang** (`251210055`)
* **Moulando Barman** (`251210069`)

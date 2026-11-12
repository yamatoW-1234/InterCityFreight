// ============================================================================
//  test_main.cpp — Unit tests for every algorithm and data structure.
//  Uses small graphs with manually verified expected answers.
//  Build via tests/run_tests.sh (clang++), which links the engine sources.
// ============================================================================
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <string>
#include <vector>

#include "graph.hpp"
#include "dijkstra.hpp"
#include "astar.hpp"
#include "bellman_ford.hpp"
#include "prim.hpp"
#include "avl_tree.hpp"
#include "b_tree.hpp"
#include "engine.hpp"

using namespace freight;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; std::cout << "  [PASS] " << msg << "\n"; } \
    else      { ++g_fail; std::cout << "  [FAIL] " << msg << "\n"; } \
} while (0)
#define CHECK_NEAR(a, b, eps, msg) CHECK(std::fabs((a) - (b)) < (eps), msg)

// ---------------------------------------------------------------------------
//  Dataset helpers (write JSON fixtures to a temp dir and load via Graph).
// ---------------------------------------------------------------------------
static const char* CONFIG = R"json({"fuel_price_per_litre":95.0,"vehicle_mileage_kmpl":8.0})json";

static std::string write_dataset(const std::string& name, const std::string& cities,
                                 const std::string& roads, const std::string& config = CONFIG) {
    namespace fs = std::filesystem;
    auto dir = fs::temp_directory_path() / ("freight_test_" + name);
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    { std::ofstream f(dir / "cities.json"); f << cities; }
    { std::ofstream f(dir / "roads.json");  f << roads; }
    if (!config.empty()) { std::ofstream f(dir / "config.json"); f << config; }
    return dir.string();
}

static int id_of(const Graph& g, const std::string& name) {
    int id = -1; g.find_city_id(name, id); return id;
}

// Small 4-city graph with known shortest paths. All speeds 10 => time ∝ distance.
static const char* SMALL4_CITIES = R"json({"cities":[
 {"id":0,"name":"A","lat":20.0,"lon":77.0},
 {"id":1,"name":"B","lat":20.0,"lon":77.0898},
 {"id":2,"name":"C","lat":20.0,"lon":77.1797},
 {"id":3,"name":"D","lat":20.0,"lon":77.2695}
]})json";

static const char* SMALL4_ROADS = R"json({"roads":[
 {"source":"A","destination":"B","highway":"T1","distance_km":10,"avg_speed_kmph":10,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"B","destination":"C","highway":"T1","distance_km":10,"avg_speed_kmph":10,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"A","destination":"C","highway":"T2","distance_km":25,"avg_speed_kmph":10,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"C","destination":"D","highway":"T1","distance_km":10,"avg_speed_kmph":10,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"A","destination":"D","highway":"T3","distance_km":50,"avg_speed_kmph":10,"toll":0,"subsidy_forward":0,"subsidy_reverse":0}
]})json";

// Financial graph with a single directional negative edge and NO negative cycle.
// fuel(100km)=1187.5; A->B net = 100+1187.5-1500 = -212.5; B->A net = 1287.5.
// Round trip A->B->A = 1075 > 0 (no negative cycle).
static const char* BFNEG_CITIES = R"json({"cities":[
 {"id":0,"name":"A","lat":20.0,"lon":77.0},
 {"id":1,"name":"B","lat":20.0,"lon":77.5},
 {"id":2,"name":"C","lat":20.0,"lon":78.0}
]})json";
static const char* BFNEG_ROADS = R"json({"roads":[
 {"source":"A","destination":"B","highway":"N1","distance_km":100,"avg_speed_kmph":50,"toll":100,"subsidy_forward":1500,"subsidy_reverse":0},
 {"source":"B","destination":"C","highway":"N1","distance_km":100,"avg_speed_kmph":50,"toll":100,"subsidy_forward":0,"subsidy_reverse":0}
]})json";

// Financial graph WITH a negative 2-cycle (A<->B both heavily subsidised).
static const char* BFCYC_ROADS = R"json({"roads":[
 {"source":"A","destination":"B","highway":"N1","distance_km":100,"avg_speed_kmph":50,"toll":100,"subsidy_forward":5000,"subsidy_reverse":5000},
 {"source":"B","destination":"C","highway":"N1","distance_km":100,"avg_speed_kmph":50,"toll":100,"subsidy_forward":0,"subsidy_reverse":0}
]})json";

// Disconnected graph: two isolated components.
static const char* DISC_CITIES = R"json({"cities":[
 {"id":0,"name":"A","lat":20.0,"lon":77.0},
 {"id":1,"name":"B","lat":20.0,"lon":77.1},
 {"id":2,"name":"C","lat":25.0,"lon":85.0},
 {"id":3,"name":"D","lat":25.0,"lon":85.1}
]})json";
static const char* DISC_ROADS = R"json({"roads":[
 {"source":"A","destination":"B","highway":"X","distance_km":10,"avg_speed_kmph":50,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"C","destination":"D","highway":"Y","distance_km":10,"avg_speed_kmph":50,"toll":0,"subsidy_forward":0,"subsidy_reverse":0}
]})json";

// Invalid dataset: one bad coordinate, one unknown-city road, one bad distance.
static const char* BAD_CITIES = R"json({"cities":[
 {"id":0,"name":"Good","lat":20.0,"lon":77.0},
 {"id":1,"name":"BadCoord","lat":999.0,"lon":77.0},
 {"id":2,"name":"AlsoGood","lat":21.0,"lon":78.0}
]})json";
static const char* BAD_ROADS = R"json({"roads":[
 {"source":"Good","destination":"AlsoGood","highway":"G","distance_km":10,"avg_speed_kmph":50,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"Good","destination":"Ghost","highway":"G","distance_km":10,"avg_speed_kmph":50,"toll":0,"subsidy_forward":0,"subsidy_reverse":0},
 {"source":"Good","destination":"AlsoGood","highway":"G","distance_km":-5,"avg_speed_kmph":50,"toll":0,"subsidy_forward":0,"subsidy_reverse":0}
]})json";

// ---------------------------------------------------------------------------
static void test_dijkstra() {
    std::cout << "\n[Dijkstra]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("dij", SMALL4_CITIES, SMALL4_ROADS), err), "load small4 graph");
    int A = id_of(g, "A"), D = id_of(g, "D");
    RouteResult r = dijkstra(g, A, D, WeightMode::Distance);
    CHECK(r.success, "A->D reachable");
    CHECK_NEAR(r.total_distance_km, 30.0, 1e-6, "shortest distance A->D == 30");
    CHECK(r.path.size() == 4 && r.path[0] == A && r.path[3] == D, "path has 4 nodes A...D");
    CHECK(r.path[1] == id_of(g, "B") && r.path[2] == id_of(g, "C"), "path is A-B-C-D");
}

static void test_astar() {
    std::cout << "\n[A* Search]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("astar", SMALL4_CITIES, SMALL4_ROADS), err), "load small4 graph");
    int A = id_of(g, "A"), D = id_of(g, "D");
    RouteResult r = astar(g, A, D);
    CHECK(r.success, "A->D reachable");
    CHECK_NEAR(r.total_time_hours, 3.0, 1e-6, "fastest time A->D == 3.0 h");
    CHECK(r.path[1] == id_of(g, "B") && r.path[2] == id_of(g, "C"), "A* path is A-B-C-D");
    // Cross-check: with uniform speed, A* (time) and Dijkstra (distance) agree.
    RouteResult d = dijkstra(g, A, D, WeightMode::Distance);
    CHECK(r.path == d.path, "A* path matches Dijkstra path (uniform speed)");
    CHECK(r.expanded_vertices >= 1, "A* recorded expanded vertices");
}

static void test_bellman_ford_negative_edge() {
    std::cout << "\n[Bellman-Ford: negative edge, no cycle]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("bfneg", BFNEG_CITIES, BFNEG_ROADS), err), "load negative-edge graph");
    CHECK(g.has_negative_financial_edge(), "graph reports a negative financial edge");
    int A = id_of(g, "A"), C = id_of(g, "C");

    RouteResult r = bellman_ford(g, A, C);
    CHECK(r.success, "Bellman-Ford finds A->C");
    CHECK(!r.negative_cycle, "no negative cycle reported");
    CHECK_NEAR(r.total_cost, 1075.0, 1e-6, "min cost A->C == 1075 (uses negative edge)");

    // Dijkstra must reject the same financial graph.
    RouteResult d = dijkstra(g, A, C, WeightMode::Financial);
    CHECK(!d.success, "Dijkstra rejects graph with negative financial edge");
}

static void test_bellman_ford_negative_cycle() {
    std::cout << "\n[Bellman-Ford: negative cycle detection]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("bfcyc", BFNEG_CITIES, BFCYC_ROADS), err), "load negative-cycle graph");
    int A = id_of(g, "A"), C = id_of(g, "C");
    RouteResult r = bellman_ford(g, A, C);
    CHECK(!r.success, "no finite route returned");
    CHECK(r.negative_cycle, "reachable negative cycle detected");
}

static void test_prim() {
    std::cout << "\n[Prim's MST]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("prim", SMALL4_CITIES, SMALL4_ROADS), err), "load small4 graph");
    std::vector<int> hubs = { id_of(g, "A"), id_of(g, "B"), id_of(g, "C"), id_of(g, "D") };
    MSTResult m = prim_mst(g, hubs);
    CHECK(m.success, "MST built over 4 hubs");
    CHECK(m.mst_edges.size() == 3, "MST has V-1 = 3 edges");
    CHECK_NEAR(m.total_distance_km, 30.0, 1e-6, "MST total distance == 30");
}

static void test_disconnected() {
    std::cout << "\n[Disconnected graph handling]\n";
    Graph g; std::string err;
    CHECK(g.load(write_dataset("disc", DISC_CITIES, DISC_ROADS), err), "load disconnected graph");
    int A = id_of(g, "A"), C = id_of(g, "C");
    RouteResult r = dijkstra(g, A, C, WeightMode::Distance);
    CHECK(!r.success, "Dijkstra reports unreachable destination");
    MSTResult m = prim_mst(g, { A, C });
    CHECK(!m.success && m.disconnected, "Prim reports disconnected hubs");
}

static void test_avl() {
    std::cout << "\n[AVL Tree]\n";
    CHECK(avl_self_check(), "avl_self_check passes");

    AVLTree<int> rr;
    rr.insert(1); rr.insert(2); rr.insert(3);
    auto pre = rr.preorder();
    CHECK(pre.size() == 3 && pre[0] == 2 && pre[1] == 1 && pre[2] == 3, "RR rotation yields root 2");

    AVLTree<int> lr;
    lr.insert(3); lr.insert(1); lr.insert(2);
    auto pre2 = lr.preorder();
    CHECK(pre2.size() == 3 && pre2[0] == 2 && pre2[1] == 1 && pre2[2] == 3, "LR rotation yields root 2");

    AVLTree<int> t;
    for (int i = 1; i <= 7; ++i) t.insert(i);
    CHECK(t.is_valid(), "AVL balanced after ascending inserts");
    CHECK(t.height() == 3, "height of 7-node AVL == 3");
    auto in = t.inorder();
    bool sorted = true;
    for (size_t i = 1; i < in.size(); ++i) if (in[i - 1] >= in[i]) sorted = false;
    CHECK(sorted, "in-order traversal is sorted");
    t.remove(4);
    CHECK(t.is_valid() && !t.contains(4), "delete keeps AVL valid");
}

static void test_btree() {
    std::cout << "\n[B-Tree]\n";
    CHECK(btree_self_check(), "btree_self_check passes");
    BTree<int, int> t(3);
    for (int i = 1; i <= 15; ++i) t.insert(i, i * 100);
    bool allFound = true;
    for (int i = 1; i <= 15; ++i) { int v = 0; if (!t.search(i, v) || v != i * 100) allFound = false; }
    CHECK(allFound, "search retrieves all 15 inserted keys");
    auto keys = t.inorder_keys();
    bool sorted = true;
    for (size_t i = 1; i < keys.size(); ++i) if (keys[i - 1] >= keys[i]) sorted = false;
    CHECK(sorted && keys.size() == 15, "B-Tree in-order is sorted with 15 keys");
    CHECK(t.height() >= 2, "B-Tree split into multiple levels");
}

static void test_invalid_dataset() {
    std::cout << "\n[Invalid dataset entries]\n";
    Graph g; std::string err;
    bool ok = g.load(write_dataset("bad", BAD_CITIES, BAD_ROADS), err);
    CHECK(ok, "load succeeds despite invalid records");
    CHECK(g.num_vertices() == 2, "invalid-coordinate city skipped (2 valid cities)");
    CHECK(g.num_undirected_roads() == 1, "unknown-city and bad-distance roads skipped");
    CHECK(!g.warnings().empty(), "validation warnings recorded");
    int id = -1;
    CHECK(!g.find_city_id("Ghost", id), "unknown city lookup fails");
}

static void test_engine_integration() {
    std::cout << "\n[Engine request handling]\n";
    Engine eng; std::string err;
    CHECK(eng.init(write_dataset("eng", SMALL4_CITIES, SMALL4_ROADS), err), "engine init");

    minijson::Json req = minijson::Json::object();
    req["action"] = minijson::Json("route");
    req["source"] = minijson::Json("A");
    req["destination"] = minijson::Json("D");
    req["objective"] = minijson::Json("shortest");
    auto resp = minijson::Json::parse(eng.handle_request(req));
    CHECK(resp.at("status").as_string() == "ok", "engine route A->D ok");
    CHECK(resp.at("algorithm").as_string() == "Dijkstra", "shortest selects Dijkstra");

    minijson::Json bad = minijson::Json::object();
    bad["action"] = minijson::Json("route");
    bad["source"] = minijson::Json("Nowhere");
    bad["destination"] = minijson::Json("D");
    bad["objective"] = minijson::Json("shortest");
    auto resp2 = minijson::Json::parse(eng.handle_request(bad));
    CHECK(resp2.at("status").as_string() == "error", "engine rejects unknown source city");
}

int main() {
    std::cout << "=== Inter-City Freight — C++ unit tests ===\n";
    test_dijkstra();
    test_astar();
    test_bellman_ford_negative_edge();
    test_bellman_ford_negative_cycle();
    test_prim();
    test_disconnected();
    test_avl();
    test_btree();
    test_invalid_dataset();
    test_engine_integration();
    std::cout << "\n=== RESULT: " << g_pass << " passed, " << g_fail << " failed ===\n";
    return g_fail == 0 ? 0 : 1;
}
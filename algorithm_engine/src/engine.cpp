// ============================================================================
//  engine.cpp — Implementation of the request dispatcher / decision controller.
// ============================================================================
#include "engine.hpp"
#include "astar.hpp"
#include "dijkstra.hpp"
#include "bellman_ford.hpp"
#include "prim.hpp"
#include "avl_tree.hpp"
#include "haversine.hpp"

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

namespace freight {

using minijson::Json;

// ---------------------------------------------------------------------------
//  Initialisation
// ---------------------------------------------------------------------------
bool Engine::init(const std::string& data_dir, std::string& err) {
    data_dir_ = data_dir;
    if (!graph_.load(data_dir, err)) return false;
    for (const auto& c : graph_.cities())
        city_index_.insert(c.name, c.id);            // build the B-Tree city index
    return true;
}

bool Engine::resolve_city(const std::string& name, int& id) {
    return city_index_.search(name, id);             // genuine B-Tree search
}

// ---------------------------------------------------------------------------
//  Internal Decision Controller — objective -> algorithm
// ---------------------------------------------------------------------------
RouteResult Engine::run_objective(const std::string& objective, int source, int dest,
                                  std::string& algorithm, std::string& reason) {
    if (objective == "fastest") {
        algorithm = "A* Search";
        reason = "Fastest-transit objective: A* uses travel-time edge weights with an "
                 "admissible Haversine straight-line heuristic.";
        return astar(graph_, source, dest);
    }
    if (objective == "shortest") {
        algorithm = "Dijkstra";
        reason = "Shortest-distance objective: Dijkstra with non-negative distance weights.";
        return dijkstra(graph_, source, dest, WeightMode::Distance);
    }
    // "cheapest" (financial) objective
    if (graph_.has_negative_financial_edge()) {
        algorithm = "Bellman-Ford";
        reason = "Minimum-cost objective with negative net-cost edges (subsidies) present: "
                 "Bellman-Ford handles negative weights and detects negative cycles.";
        return bellman_ford(graph_, source, dest);
    }
    algorithm = "Dijkstra";
    reason = "Minimum-cost objective with all non-negative costs: Dijkstra is the most "
             "efficient valid choice (Bellman-Ford would also be correct).";
    return dijkstra(graph_, source, dest, WeightMode::Financial);
}

// ---------------------------------------------------------------------------
//  Result serialization
// ---------------------------------------------------------------------------
Json Engine::route_to_json(const RouteResult& r, const std::string& objective,
                           const std::string& algorithm, const std::string& reason,
                           const std::string& src_name, const std::string& dst_name,
                           double budget, double time_limit) {
    Json o = Json::object();
    o["objective"]        = Json(objective);
    o["algorithm"]        = Json(r.algorithm.empty() ? algorithm : r.algorithm);
    o["selection_reason"] = Json(reason);

    if (!r.success) {
        o["status"]         = Json("error");
        o["message"]        = Json(r.status_message);
        o["negative_cycle"] = Json(r.negative_cycle);
        return o;
    }

    o["status"]      = Json("ok");
    o["source"]      = Json(src_name);
    o["destination"] = Json(dst_name);

    Json pathArr = Json::array();
    Json coordArr = Json::array();
    for (int id : r.path) {
        pathArr.push_back(Json(graph_.city(id).name));
        Json c = Json::array();
        c.push_back(Json(graph_.city(id).lat));
        c.push_back(Json(graph_.city(id).lon));
        coordArr.push_back(c);
    }
    o["path"]        = pathArr;
    o["path_coords"] = coordArr;

    Json edgeArr = Json::array();
    for (const Edge& e : r.path_edges) {
        Json ej = Json::object();
        ej["from"]        = Json(graph_.city(e.from).name);
        ej["to"]          = Json(graph_.city(e.to).name);
        ej["highway"]     = Json(e.highway);
        ej["distance_km"] = Json(e.distance_km);
        ej["time_hours"]  = Json(e.time_hours());
        ej["toll"]        = Json(e.toll);
        ej["fuel"]        = Json(graph_.fuel_cost(e.distance_km));
        ej["subsidy"]     = Json(e.subsidy);
        ej["cost"]        = Json(graph_.financial_cost(e));
        edgeArr.push_back(ej);
    }
    o["edges"] = edgeArr;

    o["total_distance_km"] = Json(r.total_distance_km);
    o["total_time_hours"]  = Json(r.total_time_hours);
    o["total_toll"]        = Json(r.total_toll);
    o["total_fuel"]        = Json(r.total_fuel);
    o["total_subsidy"]     = Json(r.total_subsidy);
    o["total_cost"]        = Json(r.total_cost);
    o["execution_time_ms"] = Json(r.execution_time_ms);
    o["expanded_vertices"] = Json(r.expanded_vertices);
    o["negative_cycle"]    = Json(r.negative_cycle);

    // Budget / time limits are evaluated against the produced route (post-hoc),
    // NOT solved as a multi-constraint optimisation problem.
    o["budget_limit"]      = Json(budget);
    o["time_limit_hours"]  = Json(time_limit);
    o["within_budget"]     = Json(budget > 0 ? (r.total_cost <= budget) : true);
    o["within_time"]       = Json(time_limit > 0 ? (r.total_time_hours <= time_limit) : true);
    return o;
}

Json Engine::error_json(const std::string& msg) {
    Json o = Json::object();
    o["status"]  = Json("error");
    o["message"] = Json(msg);
    return o;
}

// ---------------------------------------------------------------------------
//  Handlers
// ---------------------------------------------------------------------------
Json Engine::handle_route(const Json& req) {
    const std::string src = req.at("source").as_string("");
    const std::string dst = req.at("destination").as_string("");
    const std::string objective = req.at("objective").as_string("fastest");
    const double budget = req.at("budget_limit").as_number(0);
    const double time_limit = req.at("time_limit_hours").as_number(0);

    int s = -1, d = -1;
    if (src.empty() || !resolve_city(src, s)) return error_json("unknown source city: " + src);
    if (dst.empty() || !resolve_city(dst, d)) return error_json("unknown destination city: " + dst);

    std::string algorithm, reason;
    RouteResult r = run_objective(objective, s, d, algorithm, reason);
    return route_to_json(r, objective, algorithm, reason, src, dst, budget, time_limit);
}

Json Engine::handle_compare(const Json& req) {
    const std::string src = req.at("source").as_string("");
    const std::string dst = req.at("destination").as_string("");
    int s = -1, d = -1;
    if (!resolve_city(src, s)) return error_json("unknown source city: " + src);
    if (!resolve_city(dst, d)) return error_json("unknown destination city: " + dst);

    Json o = Json::object();
    o["status"]      = Json("ok");
    o["source"]      = Json(src);
    o["destination"] = Json(dst);

    Json routes = Json::array();
    const char* objectives[3] = {"fastest", "cheapest", "shortest"};
    for (const char* obj : objectives) {
        std::string algorithm, reason;
        RouteResult r = run_objective(obj, s, d, algorithm, reason);
        routes.push_back(route_to_json(r, obj, algorithm, reason, src, dst, 0, 0));
    }
    o["routes"] = routes;
    return o;
}

Json Engine::handle_mst(const Json& req) {
    if (!req.has("hubs") || !req.at("hubs").is_array())
        return error_json("mst requires a 'hubs' array of city names");

    std::vector<int> hubs;
    for (const auto& h : req.at("hubs").array_items()) {
        const std::string name = h.as_string("");
        int id = -1;
        if (!resolve_city(name, id)) return error_json("unknown hub city: " + name);
        hubs.push_back(id);
    }

    MSTResult m = prim_mst(graph_, hubs);

    Json o = Json::object();
    Json hubArr = Json::array();
    for (int id : m.hubs) hubArr.push_back(Json(graph_.city(id).name));
    o["hubs"]    = hubArr;
    o["status"]  = Json(m.success ? "ok" : "error");

    if (!m.success) {
        o["message"]     = Json(m.status_message);
        o["disconnected"] = Json(m.disconnected);
        Json un = Json::array();
        for (int id : m.unreachable_hubs) un.push_back(Json(graph_.city(id).name));
        o["unreachable_hubs"] = un;
        return o;
    }

    Json edgeArr = Json::array();
    for (const auto& me : m.mst_edges) {
        Json ej = Json::object();
        ej["from"]        = Json(graph_.city(me.from_hub).name);
        ej["to"]          = Json(graph_.city(me.to_hub).name);
        ej["distance_km"] = Json(me.distance_km);
        ej["cost"]        = Json(me.cost);
        Json cp = Json::array();
        Json cc = Json::array();
        for (int id : me.corridor_path) {
            cp.push_back(Json(graph_.city(id).name));
            Json c = Json::array();
            c.push_back(Json(graph_.city(id).lat));
            c.push_back(Json(graph_.city(id).lon));
            cc.push_back(c);
        }
        ej["corridor_path"]   = cp;
        ej["corridor_coords"] = cc;
        edgeArr.push_back(ej);
    }
    o["mst_edges"]         = edgeArr;
    o["total_distance_km"] = Json(m.total_distance_km);
    o["total_cost"]        = Json(m.total_cost);
    o["execution_time_ms"] = Json(m.execution_time_ms);
    o["disconnected"]      = Json(false);
    return o;
}

Json Engine::handle_network() {
    Json o = graph_.to_summary_json();
    o["status"] = Json("ok");
    return o;
}

Json Engine::handle_meta() {
    Json o = Json::object();
    o["status"]          = Json("ok");
    o["project_name"]    = Json("Inter-City Freight Logistics & Route Generator");
    o["subject"]         = Json("Design and Analysis of Algorithms (DAA)");
    o["num_cities"]      = Json(graph_.num_vertices());
    o["num_roads"]       = Json(graph_.num_undirected_roads());
    o["has_negative_financial_edge"] = Json(graph_.has_negative_financial_edge());

    Json algos = Json::array();
    {
        Json a = Json::object();
        a["name"]     = Json("A* Search");
        a["purpose"]  = Json("Fastest freight transit");
        a["weight"]   = Json("estimated travel time (distance / speed)");
        a["heuristic"]= Json("Haversine straight-line distance / max dataset speed (admissible + consistent)");
        a["time_best"]  = Json("O(E)");
        a["time_avg"]   = Json("O((V+E) log V)");
        a["time_worst"] = Json("O((V+E) log V)");
        a["space"]      = Json("O(V)");
        a["data_structure"] = Json("AVL-backed indexed min-priority queue");
        algos.push_back(a);
    }
    {
        Json a = Json::object();
        a["name"]     = Json("Dijkstra");
        a["purpose"]  = Json("Shortest distance / non-negative minimum cost");
        a["weight"]   = Json("distance, or non-negative financial cost");
        a["heuristic"]= Json("none (greedy best-first expansion)");
        a["time_best"]  = Json("O((V+E) log V)");
        a["time_avg"]   = Json("O((V+E) log V)");
        a["time_worst"] = Json("O((V+E) log V)");
        a["space"]      = Json("O(V)");
        a["data_structure"] = Json("AVL-backed indexed min-priority queue");
        algos.push_back(a);
    }
    {
        Json a = Json::object();
        a["name"]     = Json("Bellman-Ford");
        a["purpose"]  = Json("Minimum financial cost with possible negative edges (subsidies)");
        a["weight"]   = Json("toll + fuel - subsidy");
        a["heuristic"]= Json("none (dynamic programming over edge relaxations)");
        a["time_best"]  = Json("O(V x E)");
        a["time_avg"]   = Json("O(V x E)");
        a["time_worst"] = Json("O(V x E)");
        a["space"]      = Json("O(V)");
        a["data_structure"] = Json("distance + predecessor arrays");
        algos.push_back(a);
    }
    {
        Json a = Json::object();
        a["name"]     = Json("Prim's MST");
        a["purpose"]  = Json("Multi-hub minimum-cost infrastructure backbone");
        a["weight"]   = Json("shortest-path corridor distance (metric closure)");
        a["heuristic"]= Json("none (greedy nearest-hub attachment)");
        a["time_best"]  = Json("O((V+E) log V)");
        a["time_avg"]   = Json("O((V+E) log V)");
        a["time_worst"] = Json("O((V+E) log V)");
        a["space"]      = Json("O(V)");
        a["data_structure"] = Json("AVL-backed indexed min-priority queue");
        algos.push_back(a);
    }
    o["algorithms"] = algos;

    Json ds = Json::array();
    {
        Json d = Json::object();
        d["name"]  = Json("AVL Tree");
        d["role"]  = Json("backs the indexed min-priority queue used by A*, Dijkstra and Prim");
        d["ops"]   = Json("insert / delete / search / extract-min / decrease-key in O(log n) via rotations");
        ds.push_back(d);
    }
    {
        Json d = Json::object();
        d["name"]  = Json("B-Tree");
        d["role"]  = Json("city-name -> city-id index; resolves every request city via a B-Tree search");
        d["ops"]   = Json("insert-with-split / search / traversal in O(log n), configurable min degree");
        ds.push_back(d);
    }
    o["data_structures"] = ds;
    return o;
}

bool Engine::build_synthetic_graph(Graph& g, int n) {
    // Deterministic PRNG (LCG) so benchmarks are reproducible.
    unsigned long long state = 123456789ULL + (unsigned long long)n * 1000003ULL;
    auto rnd = [&]() -> double {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return (double)((state >> 33) & 0x7fffffff) / (double)0x7fffffff;
    };

    std::vector<double> lat(n), lon(n);
    Json cities = Json::object();
    Json carr = Json::array();
    for (int i = 0; i < n; ++i) {
        lat[i] = 8.0 + rnd() * 27.0;
        lon[i] = 68.0 + rnd() * 29.0;
        Json c = Json::object();
        c["id"]   = Json(i);
        c["name"] = Json("City" + std::to_string(i));
        c["lat"]  = Json(lat[i]);
        c["lon"]  = Json(lon[i]);
        carr.push_back(c);
    }
    cities["cities"] = carr;

    Json roads = Json::object();
    Json rarr = Json::array();
    auto add_road = [&](int a, int b) {
        if (a == b) return;
        double d = geo::haversine_km(lat[a], lon[a], lat[b], lon[b]);
        if (d < 1.0) d = 1.0;
        Json r = Json::object();
        r["source"]         = Json("City" + std::to_string(a));
        r["destination"]    = Json("City" + std::to_string(b));
        r["highway"]        = Json("SYN-HW");
        r["distance_km"]    = Json(d);
        r["avg_speed_kmph"] = Json(60.0 + rnd() * 30.0);
        r["toll"]           = Json(d * (2.0 + rnd() * 2.0));
        r["subsidy_forward"]= Json(0);
        r["subsidy_reverse"]= Json(0);
        rarr.push_back(r);
    };
    for (int i = 0; i + 1 < n; ++i) add_road(i, i + 1);       // backbone chain
    for (int e = 0; e < n; ++e) {                              // random shortcuts
        int a = (int)(rnd() * n); if (a >= n) a = n - 1;
        int b = (int)(rnd() * n); if (b >= n) b = n - 1;
        add_road(a, b);
    }
    roads["roads"] = rarr;

    namespace fs = std::filesystem;
    std::error_code ec;
    auto tmp = fs::temp_directory_path() / ("freight_bench_" + std::to_string(n));
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    { std::ofstream f(tmp / "cities.json"); f << cities.dump(); }
    { std::ofstream f(tmp / "roads.json");  f << roads.dump(); }

    std::string err;
    bool ok = g.load(tmp.string(), err);
    fs::remove_all(tmp, ec);
    return ok;
}

Json Engine::handle_benchmark(const Json& req) {
    std::vector<int> sizes;
    if (req.has("sizes") && req.at("sizes").is_array())
        for (const auto& s : req.at("sizes").array_items()) sizes.push_back((int)s.as_int(0));
    if (sizes.empty()) sizes = {10, 25, 50, 100, 200};

    Json o = Json::object();
    o["status"] = Json("ok");
    Json results = Json::array();
    for (int n : sizes) {
        if (n < 3) continue;
        Graph g;
        if (!build_synthetic_graph(g, n)) continue;
        const int src = 0, dst = n - 1;

        RouteResult rd = dijkstra(g, src, dst, WeightMode::Distance);
        RouteResult ra = astar(g, src, dst);
        RouteResult rb = bellman_ford(g, src, dst);
        std::vector<int> hubs = {0, n / 3, (2 * n) / 3, n - 1};
        MSTResult mp = prim_mst(g, hubs);

        Json row = Json::object();
        row["vertices"]            = Json(n);
        row["edges"]               = Json(g.num_undirected_roads());
        row["dijkstra_ms"]         = Json(rd.execution_time_ms);
        row["astar_ms"]            = Json(ra.execution_time_ms);
        row["bellman_ford_ms"]     = Json(rb.execution_time_ms);
        row["prim_ms"]             = Json(mp.execution_time_ms);
        row["dijkstra_expanded"]   = Json(rd.expanded_vertices);
        row["astar_expanded"]      = Json(ra.expanded_vertices);
        results.push_back(row);
    }
    o["results"] = results;
    return o;
}

Json Engine::handle_avl_demo(const Json& req) {
    AVLTree<int> tree;
    Json steps = Json::array();
    if (req.has("insert") && req.at("insert").is_array())
        for (const auto& v : req.at("insert").array_items()) {
            tree.insert((int)v.as_int(0));
            steps.push_back(tree.structure());
        }
    if (req.has("delete") && req.at("delete").is_array())
        for (const auto& v : req.at("delete").array_items()) {
            tree.remove((int)v.as_int(0));
            steps.push_back(tree.structure());
        }
    Json inorder = Json::array();
    for (int x : tree.inorder()) inorder.push_back(Json(x));

    Json o = Json::object();
    o["status"]  = Json("ok");
    o["steps"]   = steps;
    o["final"]   = tree.structure();
    o["inorder"] = inorder;
    o["height"]  = Json(tree.height());
    o["size"]    = Json((long long)tree.size());
    o["valid"]   = Json(tree.is_valid());
    return o;
}

Json Engine::handle_btree_demo(const Json& req) {
    const int t = (int)req.at("min_degree").as_int(3);
    BTree<int, int> tree(t);
    Json steps = Json::array();
    if (req.has("insert") && req.at("insert").is_array())
        for (const auto& v : req.at("insert").array_items()) {
            const int k = (int)v.as_int(0);
            tree.insert(k, k * 10);
            steps.push_back(tree.structure());
        }
    Json inorder = Json::array();
    for (int x : tree.inorder_keys()) inorder.push_back(Json(x));

    Json o = Json::object();
    o["status"]      = Json("ok");
    o["min_degree"]  = Json(t);
    o["steps"]       = steps;
    o["final"]       = tree.structure();
    o["inorder"]     = inorder;
    o["height"]      = Json(tree.height());
    o["size"]        = Json((long long)tree.size());
    return o;
}

Json Engine::handle_selfcheck() {
    Json o = Json::object();
    o["status"]           = Json("ok");
    o["avl_self_check"]   = Json(avl_self_check());
    o["btree_self_check"] = Json(btree_self_check());
    return o;
}

// ---------------------------------------------------------------------------
//  Dispatch
// ---------------------------------------------------------------------------
std::string Engine::handle_request(const Json& req) {
    const std::string action = req.has("action") ? req.at("action").as_string("route") : "route";
    Json result;
    try {
        if      (action == "route")       result = handle_route(req);
        else if (action == "compare")     result = handle_compare(req);
        else if (action == "mst")         result = handle_mst(req);
        else if (action == "network")     result = handle_network();
        else if (action == "meta")        result = handle_meta();
        else if (action == "benchmark")   result = handle_benchmark(req);
        else if (action == "avl_demo")    result = handle_avl_demo(req);
        else if (action == "btree_demo")  result = handle_btree_demo(req);
        else if (action == "selfcheck")   result = handle_selfcheck();
        else                              result = error_json("unknown action: " + action);
    } catch (const std::exception& e) {
        result = error_json(std::string("engine exception: ") + e.what());
    }
    return result.dump();
}

} // namespace freight
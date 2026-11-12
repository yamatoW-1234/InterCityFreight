// ============================================================================
//  engine.hpp — Request dispatcher + "Internal Decision Controller".
//
//  Responsibilities:
//   - Load the weighted graph and build a B-Tree city-name -> city-id index.
//   - Parse JSON requests and dispatch to the correct handler.
//   - Automatically select the routing algorithm for a given objective.
//   - Serialize algorithm results back to JSON.
// ============================================================================
#pragma once
#include "graph.hpp"
#include "b_tree.hpp"
#include "route_result.hpp"
#include "json.hpp"
#include <string>

namespace freight {

class Engine {
public:
    bool init(const std::string& data_dir, std::string& err);
    std::string handle_request(const minijson::Json& req);

    const Graph& graph() const { return graph_; }

private:
    // ---- action handlers ----
    minijson::Json handle_route(const minijson::Json& req);
    minijson::Json handle_compare(const minijson::Json& req);
    minijson::Json handle_mst(const minijson::Json& req);
    minijson::Json handle_network();
    minijson::Json handle_meta();
    minijson::Json handle_benchmark(const minijson::Json& req);
    minijson::Json handle_avl_demo(const minijson::Json& req);
    minijson::Json handle_btree_demo(const minijson::Json& req);
    minijson::Json handle_selfcheck();

    // ---- helpers ----
    bool resolve_city(const std::string& name, int& id);   // B-Tree lookup
    RouteResult run_objective(const std::string& objective, int source, int dest,
                              std::string& algorithm, std::string& reason);
    minijson::Json route_to_json(const RouteResult& r, const std::string& objective,
                                 const std::string& algorithm, const std::string& reason,
                                 const std::string& src_name, const std::string& dst_name,
                                 double budget, double time_limit);
    minijson::Json error_json(const std::string& msg);
    bool build_synthetic_graph(Graph& g, int n);           // for benchmarking

    Graph graph_;
    BTree<std::string, int> city_index_;                    // B-Tree city index
    std::string data_dir_;
};

} // namespace freight
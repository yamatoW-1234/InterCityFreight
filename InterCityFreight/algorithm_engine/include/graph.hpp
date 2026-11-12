// ============================================================================
//  graph.hpp — Weighted graph model of the inter-city freight network.
//  Cities are vertices; highways are directed edges (each road is stored in
//  both directions so that per-direction subsidies can differ).
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "json.hpp"

namespace freight {

enum class WeightMode { Time, Distance, Financial };

struct City {
    int         id   = -1;
    std::string name;
    double      lat  = 0.0;
    double      lon  = 0.0;
};

struct Edge {
    int         from = -1;
    int         to   = -1;
    std::string highway;
    double      distance_km     = 0.0;
    double      avg_speed_kmph  = 0.0;
    double      toll            = 0.0;
    double      subsidy         = 0.0;   // applies in THIS direction only

    double time_hours() const {
        return avg_speed_kmph > 0.0 ? distance_km / avg_speed_kmph : 0.0;
    }
};

struct FinancialConfig {
    double fuel_price_per_litre     = 95.0;
    double vehicle_mileage_kmpl     = 8.0;
    double max_speed_override_kmph  = 0.0;   // 0 => derive from dataset
};

class Graph {
public:
    // Load cities.json, roads.json and (optionally) config.json from data_dir.
    bool load(const std::string& data_dir, std::string& err);

    int num_vertices()        const { return (int)cities_.size(); }
    int num_directed_edges()  const { return (int)edges_.size(); }
    int num_undirected_roads()const { return (int)edges_.size() / 2; }

    const std::vector<City>&              cities()      const { return cities_; }
    const std::vector<Edge>&              edges()       const { return edges_; }
    const std::vector<std::vector<Edge>>& adj()         const { return adj_; }
    const City&                           city(int id)  const { return cities_[id]; }
    const FinancialConfig&                config()      const { return cfg_; }
    double                                max_speed()   const { return max_speed_; }

    bool find_city_id(const std::string& name, int& out_id) const;
    const std::unordered_map<std::string, int>& name_index() const { return name_to_id_; }

    // Cost model.
    double fuel_cost(double distance_km) const;
    double financial_cost(const Edge& e) const;       // toll + fuel - subsidy
    double edge_weight(const Edge& e, WeightMode m) const;
    bool   has_negative_financial_edge() const;

    const std::vector<std::string>& warnings() const { return warnings_; }

    // Network overview used by the dashboard /api/network endpoint.
    minijson::Json to_summary_json() const;

private:
    bool load_cities(const std::string& path, std::string& err);
    bool load_roads(const std::string& path, std::string& err);
    bool load_config(const std::string& path);              // optional file
    bool read_file(const std::string& path, std::string& out) const;

    std::vector<City>                 cities_;
    std::vector<std::vector<Edge>>    adj_;
    std::vector<Edge>                 edges_;
    std::unordered_map<std::string,int> name_to_id_;
    FinancialConfig                   cfg_;
    double                            max_speed_ = 0.0;
    std::vector<std::string>          warnings_;
};

} // namespace freight
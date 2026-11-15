// ============================================================================
//  graph.cpp — Dataset ingestion, validation and cost-model implementation.
// ============================================================================
#include "graph.hpp"
#include <fstream>
#include <sstream>
#include <cmath>

namespace freight {

bool Graph::read_file(const std::string& path, std::string& out) const {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool Graph::load(const std::string& data_dir, std::string& err) {
    warnings_.clear();
    cities_.clear();
    edges_.clear();
    adj_.clear();
    name_to_id_.clear();

    std::string dir = data_dir;
    if (!dir.empty() && dir.back() != '/') dir += '/';

    load_config(dir + "config.json");                    // optional; keeps defaults on failure
    if (!load_cities(dir + "cities.json", err)) return false;
    if (!load_roads(dir + "roads.json", err))   return false;

    adj_.assign(cities_.size(), {});
    for (const Edge& e : edges_) adj_[e.from].push_back(e);

    max_speed_ = cfg_.max_speed_override_kmph;
    for (const Edge& e : edges_)
        if (e.avg_speed_kmph > max_speed_) max_speed_ = e.avg_speed_kmph;
    if (max_speed_ <= 0.0) max_speed_ = 1.0;             // safety guard

    return true;
}

bool Graph::load_config(const std::string& path) {
    std::string text;
    if (!read_file(path, text)) return false;
    try {
        auto doc = minijson::Json::parse(text);
        if (doc.has("fuel_price_per_litre")) {
            double v = doc.at("fuel_price_per_litre").as_number(0);
            if (v > 0) cfg_.fuel_price_per_litre = v;
        }
        if (doc.has("vehicle_mileage_kmpl")) {
            double v = doc.at("vehicle_mileage_kmpl").as_number(0);
            if (v > 0) cfg_.vehicle_mileage_kmpl = v;
        }
        if (doc.has("max_speed_override_kmph") && doc.at("max_speed_override_kmph").is_number()) {
            double v = doc.at("max_speed_override_kmph").as_number(0);
            if (v > 0) cfg_.max_speed_override_kmph = v;
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool Graph::load_cities(const std::string& path, std::string& err) {
    std::string text;
    if (!read_file(path, text)) { err = "cannot read " + path; return false; }

    minijson::Json doc;
    try { doc = minijson::Json::parse(text); }
    catch (const std::exception& e) { err = "JSON parse error in " + path + ": " + e.what(); return false; }

    if (!doc.has("cities") || !doc.at("cities").is_array()) {
        err = "cities.json is missing a 'cities' array";
        return false;
    }

    for (const auto& c : doc.at("cities").array_items()) {
        if (!c.is_object()) { warnings_.push_back("skipped non-object city record"); continue; }

        City city;
        city.name = c.at("name").as_string("");
        city.lat  = c.at("lat").as_number(1000.0);
        city.lon  = c.at("lon").as_number(1000.0);

        if (city.name.empty()) { warnings_.push_back("skipped city with empty name"); continue; }
        if (city.lat < -90 || city.lat > 90 || city.lon < -180 || city.lon > 180) {
            warnings_.push_back("skipped city '" + city.name + "' (invalid coordinates)");
            continue;
        }
        if (name_to_id_.count(city.name)) {
            warnings_.push_back("duplicate city name '" + city.name + "' skipped");
            continue;
        }

        city.id = (int)cities_.size();                  // enforce id == vector index
        name_to_id_[city.name] = city.id;
        cities_.push_back(city);
    }

    if (cities_.empty()) { err = "no valid cities were loaded"; return false; }
    return true;
}

bool Graph::load_roads(const std::string& path, std::string& err) {
    std::string text;
    if (!read_file(path, text)) { err = "cannot read " + path; return false; }

    minijson::Json doc;
    try { doc = minijson::Json::parse(text); }
    catch (const std::exception& e) { err = "JSON parse error in " + path + ": " + e.what(); return false; }

    if (!doc.has("roads") || !doc.at("roads").is_array()) {
        err = "roads.json is missing a 'roads' array";
        return false;
    }

    int idx = 0;
    for (const auto& r : doc.at("roads").array_items()) {
        ++idx;
        if (!r.is_object()) {
            warnings_.push_back("road #" + std::to_string(idx) + " skipped (not an object)");
            continue;
        }

        const std::string src = r.at("source").as_string("");
        const std::string dst = r.at("destination").as_string("");
        int si = -1, di = -1;
        if (!find_city_id(src, si) || !find_city_id(dst, di)) {
            warnings_.push_back("road #" + std::to_string(idx) + " skipped (unknown city)");
            continue;
        }

        double dist  = r.at("distance_km").as_number(0);
        double speed = r.at("avg_speed_kmph").as_number(0);
        double toll  = r.at("toll").as_number(0);
        double sf    = r.at("subsidy_forward").as_number(0);
        double sr    = r.at("subsidy_reverse").as_number(0);

        if (dist <= 0 || speed <= 0) {
            warnings_.push_back("road #" + std::to_string(idx) + " skipped (invalid distance/speed)");
            continue;
        }
        if (toll < 0) { warnings_.push_back("road #" + std::to_string(idx) + " negative toll clamped to 0"); toll = 0; }
        if (sf   < 0) { warnings_.push_back("road #" + std::to_string(idx) + " negative subsidy_forward clamped to 0"); sf = 0; }
        if (sr   < 0) { warnings_.push_back("road #" + std::to_string(idx) + " negative subsidy_reverse clamped to 0"); sr = 0; }

        const std::string hw = r.at("highway").as_string("Unknown");

        edges_.push_back(Edge{si, di, hw, dist, speed, toll, sf});   // forward
        edges_.push_back(Edge{di, si, hw, dist, speed, toll, sr});   // reverse
    }

    if (edges_.empty()) { err = "no valid roads were loaded"; return false; }
    return true;
}

bool Graph::find_city_id(const std::string& name, int& out_id) const {
    auto it = name_to_id_.find(name);
    if (it == name_to_id_.end()) return false;
    out_id = it->second;
    return true;
}

double Graph::fuel_cost(double distance_km) const {
    if (cfg_.vehicle_mileage_kmpl <= 0) return 0.0;
    return distance_km / cfg_.vehicle_mileage_kmpl * cfg_.fuel_price_per_litre;
}

double Graph::financial_cost(const Edge& e) const {
    return e.toll + fuel_cost(e.distance_km) - e.subsidy;
}

double Graph::edge_weight(const Edge& e, WeightMode m) const {
    switch (m) {
        case WeightMode::Time:      return e.time_hours();
        case WeightMode::Distance:  return e.distance_km;
        case WeightMode::Financial: return financial_cost(e);
    }
    return e.distance_km;
}

bool Graph::has_negative_financial_edge() const {
    for (const Edge& e : edges_)
        if (financial_cost(e) < 0.0) return true;
    return false;
}

minijson::Json Graph::to_summary_json() const {
    using minijson::Json;

    Json o = Json::object();
    o["num_cities"]                   = Json(num_vertices());
    o["num_roads"]                    = Json(num_undirected_roads());
    o["num_directed_edges"]           = Json(num_directed_edges());
    o["has_negative_financial_edge"]  = Json(has_negative_financial_edge());
    o["fuel_price_per_litre"]         = Json(cfg_.fuel_price_per_litre);
    o["vehicle_mileage_kmpl"]         = Json(cfg_.vehicle_mileage_kmpl);
    o["max_speed_kmph"]               = Json(max_speed_);

    Json citiesArr = Json::array();
    for (const auto& c : cities_) {
        Json cj = Json::object();
        cj["id"]   = Json(c.id);
        cj["name"] = Json(c.name);
        cj["lat"]  = Json(c.lat);
        cj["lon"]  = Json(c.lon);
        citiesArr.push_back(cj);
    }
    o["cities"] = citiesArr;

    Json roadsArr = Json::array();
    for (const auto& e : edges_) {
        if (e.from > e.to) continue;                    // emit each undirected road once
        Json rj = Json::object();
        rj["source"]         = Json(cities_[e.from].name);
        rj["destination"]    = Json(cities_[e.to].name);
        rj["highway"]        = Json(e.highway);
        rj["distance_km"]    = Json(e.distance_km);
        rj["avg_speed_kmph"] = Json(e.avg_speed_kmph);
        rj["toll"]           = Json(e.toll);
        roadsArr.push_back(rj);
    }
    o["roads"] = roadsArr;

    Json warnArr = Json::array();
    for (const auto& w : warnings_) warnArr.push_back(Json(w));
    o["warnings"] = warnArr;

    return o;
}

} // namespace freight
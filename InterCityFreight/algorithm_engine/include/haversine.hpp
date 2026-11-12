// ============================================================================
//  haversine.hpp — Great-circle distance between two GPS coordinates.
//  Used as the admissible straight-line heuristic for A* search.
// ============================================================================
#pragma once
#include <cmath>

namespace geo {

constexpr double EARTH_RADIUS_KM = 6371.0088;
constexpr double PI = 3.14159265358979323846;

inline double to_rad(double deg) { return deg * PI / 180.0; }

// Great-circle distance in kilometres (Haversine formula).
inline double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double dlat = to_rad(lat2 - lat1);
    const double dlon = to_rad(lon2 - lon1);
    const double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                     std::cos(to_rad(lat1)) * std::cos(to_rad(lat2)) *
                     std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return EARTH_RADIUS_KM * c;
}

} // namespace geo
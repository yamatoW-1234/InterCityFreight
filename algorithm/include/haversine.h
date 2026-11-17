#ifndef HAVERSINE_H
#define HAVERSINE_H

#include <math.h>

#define GEO_EARTH_RADIUS_KM 6371.0088
#define GEO_PI 3.14159265358979323846

static inline double geo_to_rad(double deg) {
    return deg * GEO_PI / 180.0;
}

/* Great-circle distance in kilometres (Haversine formula). */
static inline double geo_haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double dlat = geo_to_rad(lat2 - lat1);
    const double dlon = geo_to_rad(lon2 - lon1);
    const double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
                     cos(geo_to_rad(lat1)) * cos(geo_to_rad(lat2)) *
                     sin(dlon / 2.0) * sin(dlon / 2.0);
    const double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return GEO_EARTH_RADIUS_KM * c;
}

#endif /* HAVERSINE_H */
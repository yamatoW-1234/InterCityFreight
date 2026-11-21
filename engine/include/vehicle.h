#ifndef VEHICLE_H
#define VEHICLE_H

typedef struct {
    char id[64];
    char name[96];
    double mileage_kmpl;
    double cargo_capacity_kg;
    double fuel_capacity_l;
    double max_speed_kmh;
    double toll_multiplier;
    int toll_axle_class;
} Vehicle;

#endif

#include "dp_knapsack.h"
#include <stdlib.h>
#include <string.h>

static int cmp_value_density_desc(const void *a, const void *b) {
    const CargoItem *ia = (const CargoItem *)a;
    const CargoItem *ib = (const CargoItem *)b;
    double ra = (ia->weight_kg > 0.0) ? (ia->value_inr / ia->weight_kg) : 0.0;
    double rb = (ib->weight_kg > 0.0) ? (ib->value_inr / ib->weight_kg) : 0.0;
    if (rb > ra) return 1;
    if (rb < ra) return -1;
    return 0;
}

KnapsackResult run_partial_knapsack(CargoItem *items, int count, double capacity_kg) {
    KnapsackResult res;
    memset(&res, 0, sizeof(KnapsackResult));
    res.capacity_kg = capacity_kg;
    if (!items || count <= 0 || capacity_kg <= 0.0) return res;

    if (count > MAX_CARGO_ITEMS) count = MAX_CARGO_ITEMS;
    qsort(items, (size_t)count, sizeof(CargoItem), cmp_value_density_desc);

    double remaining = capacity_kg;
    for (int i = 0; i < count; i++) {
        CargoItem it = items[i];
        if (it.weight_kg <= 0.0 || remaining <= 0.0) {
            it.taken_kg = 0.0;
            it.fraction = 0.0;
            it.allocated_value_inr = 0.0;
        } else if (it.weight_kg <= remaining) {
            it.taken_kg = it.weight_kg;
            it.fraction = 1.0;
            it.allocated_value_inr = it.value_inr;
            remaining -= it.weight_kg;
        } else {
            it.fraction = remaining / it.weight_kg;
            it.taken_kg = remaining;
            it.allocated_value_inr = it.value_inr * it.fraction;
            remaining = 0.0;
        }
        res.items[res.item_count++] = it;
        res.used_kg += it.taken_kg;
        res.total_value_inr += it.allocated_value_inr;
    }
    return res;
}

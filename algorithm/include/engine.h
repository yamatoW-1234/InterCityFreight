#ifndef ENGINE_H
#define ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include "graph.h"
#include "b_tree.h"
#include "route_result.h"
#include "cJSON.h"

typedef struct {
    Graph graph;
    BTree city_index;              /* B-Tree mapping city name (char*) -> city id (int) */
    char  data_dir[MAX_PATH_LEN];
} Engine;

/* Lifecycle */
void  engine_init_struct(Engine* eng);
void  engine_free(Engine* eng);

/* Load graph and build B-Tree city-name -> city-id index */
bool  engine_load(Engine* eng, const char* data_dir, char* err_buf, size_t err_size);

/* Parse JSON request and return a newly allocated JSON response string (caller must free) */
char* engine_handle_request(Engine* eng, const cJSON* req);

#endif /* ENGINE_H */
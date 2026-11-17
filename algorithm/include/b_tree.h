#ifndef B_TREE_H
#define B_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include "graph.h"
#include "cJSON.h"

typedef struct BTreeNode {
    bool               leaf;
    int                num_keys;
    char             (*keys)[MAX_CITY_NAME_LEN]; /* array of size 2*t - 1 */
    int*               values;                   /* array of size 2*t - 1 */
    struct BTreeNode** children;                 /* array of size 2*t     */
} BTreeNode;

typedef struct {
    int        t;     /* minimum degree (default 3 => max 5 keys per node) */
    BTreeNode* root;
    size_t     size;
} BTree;

/* Lifecycle */
void   btree_init(BTree* tree, int min_degree);
void   btree_free(BTree* tree);

/* Accessors */
int    btree_min_degree(const BTree* tree);
size_t btree_size(const BTree* tree);
bool   btree_empty(const BTree* tree);
int    btree_height(const BTree* tree);

/* Core Operations */
bool   btree_search(const BTree* tree, const char* key, int* out_val);
void   btree_insert(BTree* tree, const char* key, int val);

/* In-order traversal (caller allocates `out_keys` with capacity >= tree->size; returns count) */
size_t btree_inorder_keys(const BTree* tree, char (*out_keys)[MAX_CITY_NAME_LEN]);

/* Visualizer Snapshot & Self-Check */
cJSON* btree_structure_json(const BTree* tree);
bool   btree_self_check(void);

#endif /* B_TREE_H */
#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"

typedef struct {
    double    priority;
    long long seq; /* tie-breaker -> unique keys even with equal priority */
    int       id;  /* payload (vertex id) */
} PQEntry;

static inline int pq_entry_cmp(PQEntry a, PQEntry b) {
    if (a.priority < b.priority) return -1;
    if (a.priority > b.priority) return 1;
    if (a.seq < b.seq) return -1;
    if (a.seq > b.seq) return 1;
    return 0;
}

typedef struct AVLNode {
    PQEntry         key;
    struct AVLNode* left;
    struct AVLNode* right;
    int             height;
} AVLNode;

typedef struct {
    AVLNode* root;
    size_t   size;
} AVLTree;

/* Lifecycle */
void   avl_init(AVLTree* tree);
void   avl_clear(AVLTree* tree);

/* Accessors & Metrics */
bool   avl_empty(const AVLTree* tree);
size_t avl_size(const AVLTree* tree);
int    avl_height(const AVLTree* tree);
int    avl_balance_factor(const AVLTree* tree);

/* Core Operations */
bool   avl_contains(const AVLTree* tree, PQEntry k);
void   avl_insert(AVLTree* tree, PQEntry k);
bool   avl_remove(AVLTree* tree, PQEntry k);
bool   avl_min(const AVLTree* tree, PQEntry* out);
bool   avl_remove_min(AVLTree* tree, PQEntry* out);

/* Traversals (caller allocates `out_arr` with capacity >= tree->size; returns count) */
size_t avl_inorder(const AVLTree* tree, PQEntry* out_arr);
size_t avl_preorder(const AVLTree* tree, PQEntry* out_arr);
size_t avl_postorder(const AVLTree* tree, PQEntry* out_arr);

/* Validation & Visualizer Snapshot */
bool   avl_is_valid(const AVLTree* tree);
cJSON* avl_structure_json(const AVLTree* tree);
bool   avl_self_check(void);

#endif /* AVL_TREE_H */
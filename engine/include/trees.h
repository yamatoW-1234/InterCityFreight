#ifndef TREES_H
#define TREES_H

#include "graph.h"

/* ---------------- AVL Tree Priority Queue ---------------- */
typedef struct AVLNode {
    double key;
    int vertex;
    double aux_time;
    int height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

typedef struct {
    AVLNode *root;
    int size;
} AVLPriorityQueue;

void avl_pq_init(AVLPriorityQueue *pq);
void avl_pq_push(AVLPriorityQueue *pq, double key, int vertex, double aux_time);
int  avl_pq_pop_min(AVLPriorityQueue *pq, double *key_out, int *vertex_out, double *aux_time_out);
int  avl_pq_empty(const AVLPriorityQueue *pq);
void avl_pq_free(AVLPriorityQueue *pq);

/* ---------------- B-Tree Spatial/City Index ---------------- */
#define BTREE_T 3 /* Minimum degree (max 2*T - 1 = 5 keys per node) */

typedef struct BTreeNode {
    int keys[2 * BTREE_T - 1];
    City *values[2 * BTREE_T - 1];
    struct BTreeNode *children[2 * BTREE_T];
    int n;
    int leaf;
} BTreeNode;

typedef struct {
    BTreeNode *root;
} BTree;

void btree_init(BTree *tree);
void btree_insert(BTree *tree, int key, City *city);
City *btree_search(const BTree *tree, int key);
void btree_free(BTree *tree);

#endif

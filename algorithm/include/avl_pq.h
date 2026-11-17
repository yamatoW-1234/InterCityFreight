#ifndef AVL_PQ_H
#define AVL_PQ_H

#include <stdbool.h>
#include <stddef.h>
#include "avl_tree.h"

typedef struct {
    AVLTree   tree;
    PQEntry*  index;       /* direct O(1) lookup table indexed by vertex id */
    bool*     in_queue;    /* tracks whether vertex id is currently in queue */
    int       capacity;    /* max vertex id + 1 supported without realloc */
    long long next_seq;
} AVLPriorityQueue;

/* Lifecycle */
void   avlpq_init(AVLPriorityQueue* pq, int initial_vertex_capacity);
void   avlpq_free(AVLPriorityQueue* pq);
void   avlpq_clear(AVLPriorityQueue* pq);

/* Queue Operations */
void   avlpq_push_or_decrease(AVLPriorityQueue* pq, int id, double priority);
bool   avlpq_contains(const AVLPriorityQueue* pq, int id);
bool   avlpq_empty(const AVLPriorityQueue* pq);
size_t avlpq_size(const AVLPriorityQueue* pq);
bool   avlpq_pop_min(AVLPriorityQueue* pq, int* out_id, double* out_priority);

#endif /* AVL_PQ_H */
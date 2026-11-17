#include "avl_pq.h"
#include <stdlib.h>
#include <string.h>

static void ensure_capacity(AVLPriorityQueue* pq, int id) {
    if (id < pq->capacity) return;
    int new_cap = pq->capacity > 0 ? pq->capacity : 32;
    while (new_cap <= id) new_cap *= 2;

    pq->index    = (PQEntry*)realloc(pq->index, (size_t)new_cap * sizeof(PQEntry));
    pq->in_queue = (bool*)realloc(pq->in_queue, (size_t)new_cap * sizeof(bool));
    for (int i = pq->capacity; i < new_cap; ++i) {
        pq->in_queue[i] = false;
    }
    pq->capacity = new_cap;
}

void avlpq_init(AVLPriorityQueue* pq, int initial_vertex_capacity) {
    avl_init(&pq->tree);
    pq->capacity = initial_vertex_capacity > 0 ? initial_vertex_capacity : 32;
    pq->index    = (PQEntry*)calloc((size_t)pq->capacity, sizeof(PQEntry));
    pq->in_queue = (bool*)calloc((size_t)pq->capacity, sizeof(bool));
    pq->next_seq = 0;
}

void avlpq_free(AVLPriorityQueue* pq) {
    avl_clear(&pq->tree);
    free(pq->index);
    free(pq->in_queue);
    pq->index    = NULL;
    pq->in_queue = NULL;
    pq->capacity = 0;
    pq->next_seq = 0;
}

void avlpq_clear(AVLPriorityQueue* pq) {
    avl_clear(&pq->tree);
    if (pq->in_queue && pq->capacity > 0) {
        memset(pq->in_queue, 0, (size_t)pq->capacity * sizeof(bool));
    }
    pq->next_seq = 0;
}

void avlpq_push_or_decrease(AVLPriorityQueue* pq, int id, double priority) {
    if (id < 0) return;
    ensure_capacity(pq, id);

    if (pq->in_queue[id]) {
        if (pq->index[id].priority <= priority) return; /* not an improvement */
        avl_remove(&pq->tree, pq->index[id]);
        pq->in_queue[id] = false;
    }

    PQEntry e = {priority, pq->next_seq++, id};
    avl_insert(&pq->tree, e);
    pq->index[id]    = e;
    pq->in_queue[id] = true;
}

bool avlpq_contains(const AVLPriorityQueue* pq, int id) {
    if (id < 0 || id >= pq->capacity) return false;
    return pq->in_queue[id];
}

bool avlpq_empty(const AVLPriorityQueue* pq) {
    return avl_empty(&pq->tree);
}

size_t avlpq_size(const AVLPriorityQueue* pq) {
    return avl_size(&pq->tree);
}

bool avlpq_pop_min(AVLPriorityQueue* pq, int* out_id, double* out_priority) {
    PQEntry m;
    if (!avl_remove_min(&pq->tree, &m)) return false;
    if (m.id >= 0 && m.id < pq->capacity) {
        pq->in_queue[m.id] = false;
    }
    *out_id       = m.id;
    *out_priority = m.priority;
    return true;
}
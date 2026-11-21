#include "trees.h"
#include <stdlib.h>

static int avl_height(const AVLNode *n) {
    return n ? n->height : 0;
}

static int max_int(int a, int b) {
    return (a > b) ? a : b;
}

static AVLNode *avl_new_node(double key, int vertex, double aux_time) {
    AVLNode *n = (AVLNode *)malloc(sizeof(AVLNode));
    if (!n) return NULL;
    n->key = key;
    n->vertex = vertex;
    n->aux_time = aux_time;
    n->height = 1;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static int cmp_entry(double k1, int v1, double k2, int v2) {
    if (k1 < k2) return -1;
    if (k1 > k2) return 1;
    if (v1 < v2) return -1;
    if (v1 > v2) return 1;
    return 0;
}

static AVLNode *rotate_right(AVLNode *y) {
    AVLNode *x = y->left;
    AVLNode *t2 = x->right;
    x->right = y;
    y->left = t2;
    y->height = max_int(avl_height(y->left), avl_height(y->right)) + 1;
    x->height = max_int(avl_height(x->left), avl_height(x->right)) + 1;
    return x;
}

static AVLNode *rotate_left(AVLNode *x) {
    AVLNode *y = x->right;
    AVLNode *t2 = y->left;
    y->left = x;
    x->right = t2;
    x->height = max_int(avl_height(x->left), avl_height(x->right)) + 1;
    y->height = max_int(avl_height(y->left), avl_height(y->right)) + 1;
    return y;
}

static int get_balance(const AVLNode *n) {
    return n ? (avl_height(n->left) - avl_height(n->right)) : 0;
}

static AVLNode *avl_insert_node(AVLNode *node, double key, int vertex, double aux_time, int *inserted) {
    if (!node) {
        *inserted = 1;
        return avl_new_node(key, vertex, aux_time);
    }
    int c = cmp_entry(key, vertex, node->key, node->vertex);
    if (c < 0) {
        node->left = avl_insert_node(node->left, key, vertex, aux_time, inserted);
    } else if (c > 0) {
        node->right = avl_insert_node(node->right, key, vertex, aux_time, inserted);
    } else {
        node->aux_time = aux_time;
        return node;
    }

    node->height = 1 + max_int(avl_height(node->left), avl_height(node->right));
    int bal = get_balance(node);

    if (bal > 1 && cmp_entry(key, vertex, node->left->key, node->left->vertex) < 0)
        return rotate_right(node);
    if (bal < -1 && cmp_entry(key, vertex, node->right->key, node->right->vertex) > 0)
        return rotate_left(node);
    if (bal > 1 && cmp_entry(key, vertex, node->left->key, node->left->vertex) > 0) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }
    if (bal < -1 && cmp_entry(key, vertex, node->right->key, node->right->vertex) < 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
}

static AVLNode *avl_pop_min_node(AVLNode *node, AVLNode **min_out) {
    if (!node) return NULL;
    if (!node->left) {
        *min_out = node;
        return node->right;
    }
    node->left = avl_pop_min_node(node->left, min_out);
    node->height = 1 + max_int(avl_height(node->left), avl_height(node->right));
    int bal = get_balance(node);

    if (bal < -1 && get_balance(node->right) <= 0)
        return rotate_left(node);
    if (bal < -1 && get_balance(node->right) > 0) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }
    return node;
}

static void avl_free_nodes(AVLNode *node) {
    if (!node) return;
    avl_free_nodes(node->left);
    avl_free_nodes(node->right);
    free(node);
}

void avl_pq_init(AVLPriorityQueue *pq) {
    pq->root = NULL;
    pq->size = 0;
}

void avl_pq_push(AVLPriorityQueue *pq, double key, int vertex, double aux_time) {
    int inserted = 0;
    pq->root = avl_insert_node(pq->root, key, vertex, aux_time, &inserted);
    if (inserted) pq->size++;
}

int avl_pq_pop_min(AVLPriorityQueue *pq, double *key_out, int *vertex_out, double *aux_time_out) {
    if (!pq->root) return 0;
    AVLNode *min_node = NULL;
    pq->root = avl_pop_min_node(pq->root, &min_node);
    if (!min_node) return 0;
    if (key_out) *key_out = min_node->key;
    if (vertex_out) *vertex_out = min_node->vertex;
    if (aux_time_out) *aux_time_out = min_node->aux_time;
    free(min_node);
    pq->size--;
    return 1;
}

int avl_pq_empty(const AVLPriorityQueue *pq) {
    return pq->size == 0;
}

void avl_pq_free(AVLPriorityQueue *pq) {
    avl_free_nodes(pq->root);
    pq->root = NULL;
    pq->size = 0;
}

#include "avl_tree.h"
#include <stdlib.h>
#include <stdio.h>

static int node_height(const AVLNode* n) {
    return n ? n->height : 0;
}

static int node_bf(const AVLNode* n) {
    return n ? (node_height(n->left) - node_height(n->right)) : 0;
}

static void node_update(AVLNode* n) {
    if (!n) return;
    int lh = node_height(n->left);
    int rh = node_height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

static AVLNode* rotate_right(AVLNode* y) {
    AVLNode* x  = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left  = T2;
    node_update(y);
    node_update(x);
    return x;
}

static AVLNode* rotate_left(AVLNode* x) {
    AVLNode* y  = x->right;
    AVLNode* T2 = y->left;
    y->left  = x;
    x->right = T2;
    node_update(x);
    node_update(y);
    return y;
}

static AVLNode* balance_node(AVLNode* n) {
    node_update(n);
    int bf = node_bf(n);
    if (bf > 1) {
        if (node_bf(n->left) < 0) n->left = rotate_left(n->left); /* LR */
        return rotate_right(n);                                   /* LL */
    }
    if (bf < -1) {
        if (node_bf(n->right) > 0) n->right = rotate_right(n->right); /* RL */
        return rotate_left(n);                                        /* RR */
    }
    return n;
}

static AVLNode* create_node(PQEntry k) {
    AVLNode* n = (AVLNode*)malloc(sizeof(AVLNode));
    if (!n) return NULL;
    n->key    = k;
    n->left   = NULL;
    n->right  = NULL;
    n->height = 1;
    return n;
}

static AVLNode* insert_rec(AVLNode* n, PQEntry k, size_t* size) {
    if (!n) {
        (*size)++;
        return create_node(k);
    }
    int cmp = pq_entry_cmp(k, n->key);
    if (cmp < 0)      n->left  = insert_rec(n->left, k, size);
    else if (cmp > 0) n->right = insert_rec(n->right, k, size);
    else return n; /* duplicate ignored */
    return balance_node(n);
}

static const AVLNode* find_rec(const AVLNode* n, PQEntry k) {
    if (!n) return NULL;
    int cmp = pq_entry_cmp(k, n->key);
    if (cmp < 0) return find_rec(n->left, k);
    if (cmp > 0) return find_rec(n->right, k);
    return n;
}

static AVLNode* min_node(AVLNode* n) {
    while (n && n->left) n = n->left;
    return n;
}

static AVLNode* remove_rec(AVLNode* n, PQEntry k, size_t* size, bool* removed) {
    if (!n) return NULL;
    int cmp = pq_entry_cmp(k, n->key);
    if (cmp < 0)      n->left  = remove_rec(n->left, k, size, removed);
    else if (cmp > 0) n->right = remove_rec(n->right, k, size, removed);
    else {
        *removed = true;
        if (!n->left || !n->right) {
            AVLNode* child = n->left ? n->left : n->right;
            free(n);
            (*size)--;
            return child;
        } else {
            AVLNode* succ = min_node(n->right);
            n->key = succ->key;
            bool dummy = false;
            n->right = remove_rec(n->right, succ->key, size, &dummy);
        }
    }
    return balance_node(n);
}

static void clear_rec(AVLNode* n) {
    if (!n) return;
    clear_rec(n->left);
    clear_rec(n->right);
    free(n);
}

static void inorder_rec(const AVLNode* n, PQEntry* arr, size_t* idx) {
    if (!n) return;
    inorder_rec(n->left, arr, idx);
    arr[(*idx)++] = n->key;
    inorder_rec(n->right, arr, idx);
}

static void preorder_rec(const AVLNode* n, PQEntry* arr, size_t* idx) {
    if (!n) return;
    arr[(*idx)++] = n->key;
    preorder_rec(n->left, arr, idx);
    preorder_rec(n->right, arr, idx);
}

static void postorder_rec(const AVLNode* n, PQEntry* arr, size_t* idx) {
    if (!n) return;
    postorder_rec(n->left, arr, idx);
    postorder_rec(n->right, arr, idx);
    arr[(*idx)++] = n->key;
}

static int check_height_rec(const AVLNode* n) {
    if (!n) return 0;
    int lh = check_height_rec(n->left);
    if (lh == -1) return -1;
    int rh = check_height_rec(n->right);
    if (rh == -1) return -1;
    if (abs(lh - rh) > 1) return -1;
    return 1 + (lh > rh ? lh : rh);
}

static cJSON* node_to_json(const AVLNode* n) {
    if (!n) return cJSON_CreateNull();
    cJSON* obj = cJSON_CreateObject();
    char buf[64];
    /* If seq == -1, format as plain integer for demo mode */
    if (n->key.seq == -1) snprintf(buf, sizeof(buf), "%d", n->key.id);
    else snprintf(buf, sizeof(buf), "%.2f (id=%d)", n->key.priority, n->key.id);

    cJSON_AddStringToObject(obj, "key", buf);
    cJSON_AddNumberToObject(obj, "height", n->height);
    cJSON_AddNumberToObject(obj, "bf", node_bf(n));
    cJSON_AddItemToObject(obj, "left", node_to_json(n->left));
    cJSON_AddItemToObject(obj, "right", node_to_json(n->right));
    return obj;
}

void avl_init(AVLTree* tree) {
    tree->root = NULL;
    tree->size = 0;
}

void avl_clear(AVLTree* tree) {
    clear_rec(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

bool avl_empty(const AVLTree* tree) { return tree->size == 0; }
size_t avl_size(const AVLTree* tree) { return tree->size; }
int avl_height(const AVLTree* tree) { return node_height(tree->root); }
int avl_balance_factor(const AVLTree* tree) { return node_bf(tree->root); }

bool avl_contains(const AVLTree* tree, PQEntry k) {
    return find_rec(tree->root, k) != NULL;
}

void avl_insert(AVLTree* tree, PQEntry k) {
    tree->root = insert_rec(tree->root, k, &tree->size);
}

bool avl_remove(AVLTree* tree, PQEntry k) {
    bool removed = false;
    tree->root = remove_rec(tree->root, k, &tree->size, &removed);
    return removed;
}

bool avl_min(const AVLTree* tree, PQEntry* out) {
    if (!tree->root) return false;
    AVLNode* n = tree->root;
    while (n->left) n = n->left;
    *out = n->key;
    return true;
}

bool avl_remove_min(AVLTree* tree, PQEntry* out) {
    if (!avl_min(tree, out)) return false;
    return avl_remove(tree, *out);
}

size_t avl_inorder(const AVLTree* tree, PQEntry* out_arr) {
    size_t idx = 0;
    inorder_rec(tree->root, out_arr, &idx);
    return idx;
}

size_t avl_preorder(const AVLTree* tree, PQEntry* out_arr) {
    size_t idx = 0;
    preorder_rec(tree->root, out_arr, &idx);
    return idx;
}

size_t avl_postorder(const AVLTree* tree, PQEntry* out_arr) {
    size_t idx = 0;
    postorder_rec(tree->root, out_arr, &idx);
    return idx;
}

bool avl_is_valid(const AVLTree* tree) {
    return check_height_rec(tree->root) != -1;
}

cJSON* avl_structure_json(const AVLTree* tree) {
    return node_to_json(tree->root);
}

bool avl_self_check(void) {
    AVLTree t;
    avl_init(&t);
    const int vals[] = {10, 20, 30, 40, 50, 25};
    for (int i = 0; i < 6; ++i) {
        PQEntry e = {(double)vals[i], -1, vals[i]};
        avl_insert(&t, e);
    }

    bool ok = avl_is_valid(&t) && avl_size(&t) == 6;

    PQEntry in[6];
    size_t cnt = avl_inorder(&t, in);
    for (size_t i = 1; i < cnt; ++i) {
        if (in[i - 1].priority >= in[i].priority) ok = false;
    }

    PQEntry probe25 = {25.0, -1, 25};
    PQEntry probe99 = {99.0, -1, 99};
    ok = ok && avl_contains(&t, probe25) && !avl_contains(&t, probe99);

    PQEntry probe40 = {40.0, -1, 40};
    avl_remove(&t, probe40);
    ok = ok && avl_is_valid(&t) && avl_size(&t) == 5 && !avl_contains(&t, probe40);

    PQEntry m;
    ok = ok && avl_min(&t, &m) && m.id == 10;

    avl_clear(&t);
    return ok;
}
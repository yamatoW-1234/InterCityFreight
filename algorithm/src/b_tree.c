#include "b_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static BTreeNode* btree_create_node(int t, bool leaf) {
    BTreeNode* n = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (!n) return NULL;
    n->leaf     = leaf;
    n->num_keys = 0;
    n->keys     = (char (*)[MAX_CITY_NAME_LEN])calloc((size_t)(2 * t - 1), MAX_CITY_NAME_LEN);
    n->values   = (int*)calloc((size_t)(2 * t - 1), sizeof(int));
    n->children = (BTreeNode**)calloc((size_t)(2 * t), sizeof(BTreeNode*));
    return n;
}

static void btree_destroy_node(BTreeNode* n) {
    if (!n) return;
    if (!n->leaf) {
        for (int i = 0; i <= n->num_keys; ++i) {
            btree_destroy_node(n->children[i]);
        }
    }
    free(n->keys);
    free(n->values);
    free(n->children);
    free(n);
}

static int btree_height_rec(const BTreeNode* n) {
    if (!n) return 0;
    if (n->leaf) return 1;
    return 1 + btree_height_rec(n->children[0]);
}

static void btree_split_child(BTree* tree, BTreeNode* x, int i) {
    int t = tree->t;
    BTreeNode* y = x->children[i];
    BTreeNode* z = btree_create_node(t, y->leaf);

    char median_key[MAX_CITY_NAME_LEN];
    strncpy(median_key, y->keys[t - 1], MAX_CITY_NAME_LEN - 1);
    median_key[MAX_CITY_NAME_LEN - 1] = '\0';
    int median_val = y->values[t - 1];

    z->num_keys = t - 1;
    for (int j = 0; j < t - 1; ++j) {
        memcpy(z->keys[j], y->keys[j + t], MAX_CITY_NAME_LEN);
        z->values[j] = y->values[j + t];
    }
    if (!y->leaf) {
        for (int j = 0; j < t; ++j) {
            z->children[j] = y->children[j + t];
        }
    }
    y->num_keys = t - 1;

    for (int j = x->num_keys; j >= i + 1; --j) {
        x->children[j + 1] = x->children[j];
    }
    x->children[i + 1] = z;

    for (int j = x->num_keys - 1; j >= i; --j) {
        memcpy(x->keys[j + 1], x->keys[j], MAX_CITY_NAME_LEN);
        x->values[j + 1] = x->values[j];
    }
    memcpy(x->keys[i], median_key, MAX_CITY_NAME_LEN);
    x->values[i] = median_val;
    x->num_keys++;
}

static void btree_insert_nonfull(BTree* tree, BTreeNode* x, const char* k, int v) {
    int pos = 0;
    while (pos < x->num_keys && strcmp(x->keys[pos], k) < 0) {
        pos++;
    }
    if (pos < x->num_keys && strcmp(x->keys[pos], k) == 0) {
        x->values[pos] = v; /* update existing */
        return;
    }

    if (x->leaf) {
        for (int j = x->num_keys - 1; j >= pos; --j) {
            memcpy(x->keys[j + 1], x->keys[j], MAX_CITY_NAME_LEN);
            x->values[j + 1] = x->values[j];
        }
        strncpy(x->keys[pos], k, MAX_CITY_NAME_LEN - 1);
        x->keys[pos][MAX_CITY_NAME_LEN - 1] = '\0';
        x->values[pos] = v;
        x->num_keys++;
        tree->size++;
        return;
    }

    if (x->children[pos]->num_keys == 2 * tree->t - 1) {
        btree_split_child(tree, x, pos);
        int cmp = strcmp(x->keys[pos], k);
        if (cmp < 0) pos++;
        else if (cmp == 0) {
            x->values[pos] = v;
            return;
        }
    }
    btree_insert_nonfull(tree, x->children[pos], k, v);
}

static void btree_inorder_rec(const BTreeNode* n, char (*out_keys)[MAX_CITY_NAME_LEN], size_t* idx) {
    if (!n) return;
    for (int i = 0; i < n->num_keys; ++i) {
        if (!n->leaf) btree_inorder_rec(n->children[i], out_keys, idx);
        memcpy(out_keys[(*idx)++], n->keys[i], MAX_CITY_NAME_LEN);
    }
    if (!n->leaf) btree_inorder_rec(n->children[n->num_keys], out_keys, idx);
}

static cJSON* btree_node_to_json(const BTreeNode* n) {
    if (!n) return cJSON_CreateNull();
    cJSON* obj = cJSON_CreateObject();
    cJSON* keysArr = cJSON_CreateArray();
    for (int i = 0; i < n->num_keys; ++i) {
        cJSON_AddItemToArray(keysArr, cJSON_CreateString(n->keys[i]));
    }
    cJSON_AddItemToObject(obj, "keys", keysArr);
    cJSON_AddBoolToObject(obj, "leaf", n->leaf);
    if (!n->leaf) {
        cJSON* ch = cJSON_CreateArray();
        for (int i = 0; i <= n->num_keys; ++i) {
            cJSON_AddItemToArray(ch, btree_node_to_json(n->children[i]));
        }
        cJSON_AddItemToObject(obj, "children", ch);
    }
    return obj;
}

void btree_init(BTree* tree, int min_degree) {
    tree->t    = min_degree < 2 ? 2 : min_degree;
    tree->root = btree_create_node(tree->t, true);
    tree->size = 0;
}

void btree_free(BTree* tree) {
    btree_destroy_node(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

int btree_min_degree(const BTree* tree) { return tree->t; }
size_t btree_size(const BTree* tree) { return tree->size; }
bool btree_empty(const BTree* tree) { return tree->size == 0; }
int btree_height(const BTree* tree) { return btree_height_rec(tree->root); }

bool btree_search(const BTree* tree, const char* key, int* out_val) {
    const BTreeNode* n = tree->root;
    while (n) {
        int i = 0;
        while (i < n->num_keys && strcmp(n->keys[i], key) < 0) {
            i++;
        }
        if (i < n->num_keys && strcmp(n->keys[i], key) == 0) {
            *out_val = n->values[i];
            return true;
        }
        if (n->leaf) return false;
        n = n->children[i];
    }
    return false;
}

void btree_insert(BTree* tree, const char* key, int val) {
    BTreeNode* r = tree->root;
    if (r->num_keys == 2 * tree->t - 1) {
        BTreeNode* s = btree_create_node(tree->t, false);
        tree->root = s;
        s->children[0] = r;
        btree_split_child(tree, s, 0);
        btree_insert_nonfull(tree, s, key, val);
    } else {
        btree_insert_nonfull(tree, r, key, val);
    }
}

size_t btree_inorder_keys(const BTree* tree, char (*out_keys)[MAX_CITY_NAME_LEN]) {
    size_t idx = 0;
    btree_inorder_rec(tree->root, out_keys, &idx);
    return idx;
}

cJSON* btree_structure_json(const BTree* tree) {
    return btree_node_to_json(tree->root);
}

bool btree_self_check(void) {
    BTree t;
    btree_init(&t, 3);
    char buf[32];
    for (int i = 1; i <= 20; ++i) {
        snprintf(buf, sizeof(buf), "%02d", i);
        btree_insert(&t, buf, i * 10);
    }

    bool ok = btree_size(&t) == 20;
    for (int i = 1; i <= 20; ++i) {
        snprintf(buf, sizeof(buf), "%02d", i);
        int v = 0;
        if (!btree_search(&t, buf, &v) || v != i * 10) ok = false;
    }
    int missing = 0;
    ok = ok && !btree_search(&t, "999", &missing);

    char keys[20][MAX_CITY_NAME_LEN];
    size_t cnt = btree_inorder_keys(&t, keys);
    ok = ok && cnt == 20;
    for (size_t i = 1; i < cnt; ++i) {
        if (strcmp(keys[i - 1], keys[i]) >= 0) ok = false;
    }

    btree_free(&t);
    return ok;
}
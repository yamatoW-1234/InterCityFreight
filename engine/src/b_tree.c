#include "trees.h"
#include <stdlib.h>
#include <string.h>

static BTreeNode *btree_create_node(int leaf) {
    BTreeNode *node = (BTreeNode *)calloc(1, sizeof(BTreeNode));
    if (node) node->leaf = leaf;
    return node;
}

void btree_init(BTree *tree) {
    tree->root = btree_create_node(1);
}

static void btree_split_child(BTreeNode *parent, int i, BTreeNode *y) {
    BTreeNode *z = btree_create_node(y->leaf);
    z->n = BTREE_T - 1;

    for (int j = 0; j < BTREE_T - 1; j++) {
        z->keys[j] = y->keys[j + BTREE_T];
        z->values[j] = y->values[j + BTREE_T];
    }
    if (!y->leaf) {
        for (int j = 0; j < BTREE_T; j++) {
            z->children[j] = y->children[j + BTREE_T];
        }
    }
    y->n = BTREE_T - 1;

    for (int j = parent->n; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[i + 1] = z;

    for (int j = parent->n - 1; j >= i; j--) {
        parent->keys[j + 1] = parent->keys[j];
        parent->values[j + 1] = parent->values[j];
    }
    parent->keys[i] = y->keys[BTREE_T - 1];
    parent->values[i] = y->values[BTREE_T - 1];
    parent->n++;
}

static void btree_insert_nonfull(BTreeNode *x, int key, City *city) {
    int i = x->n - 1;
    if (x->leaf) {
        while (i >= 0 && x->keys[i] > key) {
            x->keys[i + 1] = x->keys[i];
            x->values[i + 1] = x->values[i];
            i--;
        }
        x->keys[i + 1] = key;
        x->values[i + 1] = city;
        x->n++;
    } else {
        while (i >= 0 && x->keys[i] > key) i--;
        i++;
        if (x->children[i]->n == 2 * BTREE_T - 1) {
            btree_split_child(x, i, x->children[i]);
            if (x->keys[i] < key) i++;
        }
        btree_insert_nonfull(x->children[i], key, city);
    }
}

void btree_insert(BTree *tree, int key, City *city) {
    if (!tree->root) tree->root = btree_create_node(1);
    BTreeNode *r = tree->root;
    if (r->n == 2 * BTREE_T - 1) {
        BTreeNode *s = btree_create_node(0);
        tree->root = s;
        s->children[0] = r;
        btree_split_child(s, 0, r);
        int i = (s->keys[0] < key) ? 1 : 0;
        btree_insert_nonfull(s->children[i], key, city);
    } else {
        btree_insert_nonfull(r, key, city);
    }
}

static City *btree_search_node(const BTreeNode *x, int key) {
    if (!x) return NULL;
    int i = 0;
    while (i < x->n && key > x->keys[i]) i++;
    if (i < x->n && key == x->keys[i]) return x->values[i];
    if (x->leaf) return NULL;
    return btree_search_node(x->children[i], key);
}

City *btree_search(const BTree *tree, int key) {
    return btree_search_node(tree->root, key);
}

static void btree_free_node(BTreeNode *x) {
    if (!x) return;
    if (!x->leaf) {
        for (int i = 0; i <= x->n; i++) {
            btree_free_node(x->children[i]);
        }
    }
    free(x);
}

void btree_free(BTree *tree) {
    btree_free_node(tree->root);
    tree->root = NULL;
}

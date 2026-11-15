// ============================================================================
//  avl_tree.hpp — AVL self-balancing binary search tree (independent impl.)
//
//  Supports: insertion, deletion, search, height, balance factor,
//            LL / RR / LR / RL rotations, and in/pre/post-order traversal.
//
//  This tree backs the indexed min-priority queue used by A* and Dijkstra
//  (see avl_pq.hpp). It is a from-scratch implementation — it is NOT a
//  wrapper around std::set or std::priority_queue.
// ============================================================================
#pragma once
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include "json.hpp"

namespace freight {

template <typename Key, typename Compare = std::less<Key>>
class AVLTree {
public:
    struct Node {
        Key    key;
        Node*  left   = nullptr;
        Node*  right  = nullptr;
        int    height = 1;
        explicit Node(const Key& k) : key(k) {}
    };

    AVLTree() : root_(nullptr), size_(0), comp_() {}
    explicit AVLTree(Compare comp) : root_(nullptr), size_(0), comp_(comp) {}
    ~AVLTree() { clear(); }

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    bool   empty()  const { return size_ == 0; }
    size_t size()   const { return size_; }
    int    height() const { return height(root_); }
    int    balance_factor() const { return balance_factor(root_); }

    bool contains(const Key& k) const { return find(root_, k) != nullptr; }

    void insert(const Key& k) { root_ = insert(root_, k); }

    bool remove(const Key& k) {
        if (!contains(k)) return false;
        root_ = remove(root_, k);
        return true;
    }

    bool min(Key& out) const {
        if (!root_) return false;
        Node* n = root_;
        while (n->left) n = n->left;
        out = n->key;
        return true;
    }

    Key remove_min() {
        Key m;
        min(m);
        root_ = remove(root_, m);
        return m;
    }

    std::vector<Key> inorder()   const { std::vector<Key> v; inorder(root_, v);   return v; }
    std::vector<Key> preorder()  const { std::vector<Key> v; preorder(root_, v);  return v; }
    std::vector<Key> postorder() const { std::vector<Key> v; postorder(root_, v); return v; }

    // Verify the AVL balance + BST-ordering invariants (used by tests).
    bool is_valid() const { return check_height(root_) != -1; }

    void clear() { clear(root_); root_ = nullptr; size_ = 0; }

    // Nested JSON snapshot for the data-structure visualizer.
    minijson::Json structure() const { return node_json(root_); }

private:
    Node*   root_;
    size_t  size_;
    Compare comp_;

    int height(Node* n) const { return n ? n->height : 0; }
    int balance_factor(Node* n) const { return n ? height(n->left) - height(n->right) : 0; }
    void update(Node* n) { if (n) n->height = 1 + std::max(height(n->left), height(n->right)); }
    bool less(const Key& a, const Key& b) const { return comp_(a, b); }

    // LL rotation (right rotation around y).
    Node* rotate_right(Node* y) {
        Node* x  = y->left;
        Node* T2 = x->right;
        x->right = y;
        y->left  = T2;
        update(y);
        update(x);
        return x;
    }

    // RR rotation (left rotation around x).
    Node* rotate_left(Node* x) {
        Node* y  = x->right;
        Node* T2 = y->left;
        y->left  = x;
        x->right = T2;
        update(x);
        update(y);
        return y;
    }

    // Rebalance a node using LL / LR / RR / RL cases.
    Node* balance(Node* n) {
        update(n);
        int bf = balance_factor(n);
        if (bf > 1) {
            if (balance_factor(n->left) < 0) n->left = rotate_left(n->left);   // LR
            return rotate_right(n);                                             // LL
        }
        if (bf < -1) {
            if (balance_factor(n->right) > 0) n->right = rotate_right(n->right); // RL
            return rotate_left(n);                                               // RR
        }
        return n;
    }

    Node* insert(Node* n, const Key& k) {
        if (!n) { ++size_; return new Node(k); }
        if (less(k, n->key))        n->left  = insert(n->left, k);
        else if (less(n->key, k))   n->right = insert(n->right, k);
        else return n;                                   // duplicate keys are ignored
        return balance(n);
    }

    Node* find(Node* n, const Key& k) const {
        if (!n) return nullptr;
        if (less(k, n->key))      return find(n->left, k);
        if (less(n->key, k))      return find(n->right, k);
        return n;
    }

    Node* min_node(Node* n) const { while (n->left) n = n->left; return n; }

    Node* remove(Node* n, const Key& k) {
        if (!n) return nullptr;
        if (less(k, n->key))      n->left  = remove(n->left, k);
        else if (less(n->key, k)) n->right = remove(n->right, k);
        else {
            Node* to_delete = n;
            if (!n->left || !n->right) {
                n = n->left ? n->left : n->right;
                delete to_delete;
                --size_;
            } else {
                Node* succ = min_node(n->right);
                n->key = succ->key;
                n->right = remove(n->right, succ->key);
            }
        }
        if (!n) return nullptr;
        return balance(n);
    }

    void inorder(Node* n, std::vector<Key>& v) const {
        if (!n) return; inorder(n->left, v); v.push_back(n->key); inorder(n->right, v);
    }
    void preorder(Node* n, std::vector<Key>& v) const {
        if (!n) return; v.push_back(n->key); preorder(n->left, v); preorder(n->right, v);
    }
    void postorder(Node* n, std::vector<Key>& v) const {
        if (!n) return; postorder(n->left, v); postorder(n->right, v); v.push_back(n->key);
    }

    int check_height(Node* n) const {
        if (!n) return 0;
        int lh = check_height(n->left);  if (lh == -1) return -1;
        int rh = check_height(n->right); if (rh == -1) return -1;
        if (std::abs(lh - rh) > 1) return -1;
        return 1 + std::max(lh, rh);
    }

    void clear(Node* n) { if (!n) return; clear(n->left); clear(n->right); delete n; }

    minijson::Json node_json(Node* n) const {
        if (!n) return minijson::Json(nullptr);
        std::ostringstream ss; ss << n->key;
        minijson::Json o = minijson::Json::object();
        o["key"]    = minijson::Json(ss.str());
        o["height"] = minijson::Json(n->height);
        o["bf"]     = minijson::Json(balance_factor(n));
        o["left"]   = node_json(n->left);
        o["right"]  = node_json(n->right);
        return o;
    }
};

// Builds a small AVL tree, exercises insert/remove/rotations and validates it.
bool avl_self_check();

} // namespace freight
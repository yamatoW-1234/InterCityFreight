// ============================================================================
//  b_tree.hpp — B-Tree (order configurable) for city / road record indexing.
//
//  Templated on Key and Value with a configurable minimum degree t.
//  Supports: insert (with proactive node splitting), search, in-order
//  traversal, height, and a nested-JSON structure dump for the visualizer.
//
//  In this project the engine builds a BTree<string,int> mapping
//  city-name -> city-id and resolves every request through a B-Tree search.
// ============================================================================
#pragma once
#include <vector>
#include <string>
#include <functional>
#include <sstream>
#include "json.hpp"

namespace freight {

template <typename Key, typename Value, typename Compare = std::less<Key>>
class BTree {
public:
    explicit BTree(int min_degree = 3, Compare comp = Compare())
        : t_(min_degree < 2 ? 2 : min_degree), comp_(comp), root_(new Node(true)), size_(0) {}

    ~BTree() { destroy(root_); }

    BTree(const BTree&) = delete;
    BTree& operator=(const BTree&) = delete;

    int    min_degree() const { return t_; }
    size_t size()       const { return size_; }
    bool   empty()      const { return size_ == 0; }
    int    height()     const { return height(root_); }

    bool search(const Key& k, Value& out) const {
        Node* n = root_;
        while (n) {
            int i = 0;
            int nkeys = (int)n->keys.size();
            while (i < nkeys && less(k, n->keys[i])) ++i;
            if (i < nkeys && equal(k, n->keys[i])) { out = n->values[i]; return true; }
            if (n->leaf) return false;
            n = n->children[i];
        }
        return false;
    }

    void insert(const Key& k, const Value& v) {
        Node* r = root_;
        if ((int)r->keys.size() == 2 * t_ - 1) {
            Node* s = new Node(false);
            root_ = s;
            s->children.push_back(r);
            split_child(s, 0);
            insert_nonfull(s, k, v);
        } else {
            insert_nonfull(r, k, v);
        }
    }

    std::vector<Key> inorder_keys() const { std::vector<Key> v; inorder(root_, v); return v; }

    // Nested JSON snapshot for the data-structure visualizer.
    minijson::Json structure() const { return node_json(root_); }

private:
    struct Node {
        bool leaf;
        std::vector<Key>    keys;
        std::vector<Value>  values;
        std::vector<Node*>  children;
        explicit Node(bool l) : leaf(l) {}
    };

    int     t_;
    Compare comp_;
    Node*   root_;
    size_t  size_;

    bool less(const Key& a, const Key& b)  const { return comp_(a, b); }
    bool equal(const Key& a, const Key& b) const { return !comp_(a, b) && !comp_(b, a); }

    int height(Node* n) const {
        if (!n) return 0;
        if (n->leaf) return 1;
        return 1 + height(n->children.front());
    }

    void split_child(Node* x, int i) {
        Node* y = x->children[i];                 // full node: 2t-1 keys
        Node* z = new Node(y->leaf);
        Key   median_key = y->keys[t_ - 1];
        Value median_val = y->values[t_ - 1];

        for (int j = t_; j <= 2 * t_ - 2; ++j) {  // last t-1 keys move to z
            z->keys.push_back(y->keys[j]);
            z->values.push_back(y->values[j]);
        }
        if (!y->leaf)
            for (int j = t_; j <= 2 * t_ - 1; ++j) z->children.push_back(y->children[j]);

        y->keys.resize(t_ - 1);                   // y keeps first t-1 keys
        y->values.resize(t_ - 1);
        if (!y->leaf) y->children.resize(t_);

        x->keys.insert(x->keys.begin() + i, median_key);
        x->values.insert(x->values.begin() + i, median_val);
        x->children.insert(x->children.begin() + i + 1, z);
    }

    void insert_nonfull(Node* x, const Key& k, const Value& v) {
        int n = (int)x->keys.size();
        int pos = 0;
        while (pos < n && less(x->keys[pos], k)) ++pos;
        if (pos < n && equal(x->keys[pos], k)) { x->values[pos] = v; return; } // update

        if (x->leaf) {
            x->keys.insert(x->keys.begin() + pos, k);
            x->values.insert(x->values.begin() + pos, v);
            ++size_;
            return;
        }

        if ((int)x->children[pos]->keys.size() == 2 * t_ - 1) {
            split_child(x, pos);
            if (less(x->keys[pos], k)) ++pos;
            else if (equal(x->keys[pos], k)) { x->values[pos] = v; return; }
        }
        insert_nonfull(x->children[pos], k, v);
    }

    void inorder(Node* n, std::vector<Key>& out) const {
        if (!n) return;
        int nkeys = (int)n->keys.size();
        for (int i = 0; i < nkeys; ++i) {
            if (!n->leaf) inorder(n->children[i], out);
            out.push_back(n->keys[i]);
        }
        if (!n->leaf) inorder(n->children[nkeys], out);
    }

    void destroy(Node* n) {
        if (!n) return;
        if (!n->leaf) for (Node* c : n->children) destroy(c);
        delete n;
    }

    minijson::Json node_json(Node* n) const {
        if (!n) return minijson::Json(nullptr);
        minijson::Json o = minijson::Json::object();
        minijson::Json keysArr = minijson::Json::array();
        for (const auto& k : n->keys) {
            std::ostringstream ss; ss << k;
            keysArr.push_back(minijson::Json(ss.str()));
        }
        o["keys"] = keysArr;
        o["leaf"] = minijson::Json(n->leaf);
        if (!n->leaf) {
            minijson::Json ch = minijson::Json::array();
            for (Node* c : n->children) ch.push_back(node_json(c));
            o["children"] = ch;
        }
        return o;
    }
};

// Builds a small B-Tree, exercises insert/search/split and validates ordering.
bool btree_self_check();

} // namespace freight
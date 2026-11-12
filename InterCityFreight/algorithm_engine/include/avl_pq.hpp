// ============================================================================
//  avl_pq.hpp — Indexed min-priority queue backed by the AVL tree.
//
//  Keys are (priority, sequence) pairs so that duplicate priorities stay
//  distinct. An auxiliary hash index maps vertex-id -> current entry so that
//  decrease-key / delete run in O(log n) via real AVL insert / delete.
//  This is the queue that powers A* and Dijkstra in this project.
// ============================================================================
#pragma once
#include "avl_tree.hpp"
#include <unordered_map>

namespace freight {

struct PQEntry {
    double    priority = 0.0;
    long long seq      = 0;     // tie-breaker -> unique keys even with equal priority
    int       id       = -1;    // payload (vertex id)

    bool operator<(const PQEntry& o) const {
        if (priority != o.priority) return priority < o.priority;
        return seq < o.seq;
    }
};

class AVLPriorityQueue {
public:
    // Insert id with the given priority, or decrease its priority if already present.
    void push_or_decrease(int id, double priority);

    bool   contains(int id) const { return index_.count(id) > 0; }
    bool   empty() const { return tree_.empty(); }
    size_t size()  const { return tree_.size(); }

    // Remove and return the minimum-priority element.
    bool pop_min(int& out_id, double& out_priority);

    void clear() { tree_.clear(); index_.clear(); next_seq_ = 0; }

private:
    AVLTree<PQEntry>               tree_;
    std::unordered_map<int, PQEntry> index_;
    long long                      next_seq_ = 0;
};

} // namespace freight
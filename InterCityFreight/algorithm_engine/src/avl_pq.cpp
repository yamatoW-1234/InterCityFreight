// ============================================================================
//  avl_pq.cpp — AVL-backed indexed min-priority queue implementation.
// ============================================================================
#include "avl_pq.hpp"

namespace freight {

void AVLPriorityQueue::push_or_decrease(int id, double priority) {
    auto it = index_.find(id);
    if (it != index_.end()) {
        if (it->second.priority <= priority) return;   // not an improvement
        tree_.remove(it->second);                       // genuine AVL delete (rotations)
        index_.erase(it);
    }
    PQEntry e{priority, next_seq_++, id};
    tree_.insert(e);                                    // genuine AVL insert (rotations)
    index_[id] = e;
}

bool AVLPriorityQueue::pop_min(int& out_id, double& out_priority) {
    PQEntry m;
    if (!tree_.min(m)) return false;
    tree_.remove(m);
    index_.erase(m.id);
    out_id = m.id;
    out_priority = m.priority;
    return true;
}

} // namespace freight
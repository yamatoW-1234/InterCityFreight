// ============================================================================
//  b_tree.cpp — Explicit instantiations + a self-check used by tests/demo.
// ============================================================================
#include "b_tree.hpp"

namespace freight {

template class BTree<std::string, int>;
template class BTree<int, int>;

bool btree_self_check() {
    BTree<int, int> t(3);
    for (int i = 1; i <= 20; ++i) t.insert(i, i * 10);

    bool ok = t.size() == 20;
    for (int i = 1; i <= 20; ++i) {
        int v = 0;
        if (!t.search(i, v) || v != i * 10) ok = false;
    }
    int missing = 0;
    ok = ok && !t.search(999, missing);

    auto keys = t.inorder_keys();
    ok = ok && keys.size() == 20;
    for (size_t i = 0; i < keys.size(); ++i)
        if (keys[i] != (int)i + 1) ok = false;

    return ok;
}

} // namespace freight
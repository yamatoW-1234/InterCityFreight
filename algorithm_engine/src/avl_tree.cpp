// ============================================================================
//  avl_tree.cpp — Explicit instantiations + a self-check used by tests/demo.
// ============================================================================
#include "avl_tree.hpp"

namespace freight {

template class AVLTree<int>;
template class AVLTree<long long>;

bool avl_self_check() {
    AVLTree<int> t;
    const int vals[] = {10, 20, 30, 40, 50, 25};
    for (int v : vals) t.insert(v);

    bool ok = t.is_valid() && t.size() == 6;

    auto in = t.inorder();
    for (size_t i = 1; i < in.size(); ++i)
        if (in[i - 1] >= in[i]) ok = false;              // must be strictly sorted

    ok = ok && t.contains(25) && !t.contains(99);

    t.remove(40);
    ok = ok && t.is_valid() && t.size() == 5 && !t.contains(40);

    int m = -1;
    t.min(m);
    ok = ok && (m == 10);
    return ok;
}

} // namespace freight
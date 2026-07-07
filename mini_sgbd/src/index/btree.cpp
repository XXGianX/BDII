#include "btree.h"
#include <iostream>
#include <queue>

BTree::BTree(BufferManager& bm, PageId root_page_id)
    : bm_(bm), root_page_id_(root_page_id)
{}

PageId BTree::create_node(bool is_leaf, PageId parent_id) {
    PageId pid;
    Page* page = bm_.new_page(pid);
    BTreeNode node(page->data_);
    node.init(pid, is_leaf, parent_id);
    bm_.unpin_page(pid, true);
    return pid;
}

PageId BTree::find_leaf(int32_t key) {
    if (root_page_id_ == INVALID_PAGE) return INVALID_PAGE;

    PageId current = root_page_id_;
    while (true) {
        Page* page = bm_.fetch_page(current);
        BTreeNode node(page->data_);

        if (node.is_leaf()) {
            bm_.unpin_page(current, false);
            return current;
        }

        uint32_t n   = node.num_keys();
        int32_t* ks  = node.keys();
        PageId*  cs  = node.children();

        uint32_t i = 0;
        while (i < n && key >= ks[i]) i++;
        PageId next = cs[i];
        bm_.unpin_page(current, false);
        current = next;
    }
}

std::optional<RID> BTree::search(int32_t key) {
    if (root_page_id_ == INVALID_PAGE) return std::nullopt;

    PageId leaf_pid = find_leaf(key);
    if (leaf_pid == INVALID_PAGE) return std::nullopt;

    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    uint32_t n    = node.num_keys();
    int32_t* keys = node.keys();
    RID*     rids = node.rids();

    for (uint32_t i = 0; i < n; i++) {
        if (keys[i] == key) {
            RID result = rids[i];
            bm_.unpin_page(leaf_pid, false);
            return result;
        }
    }
    bm_.unpin_page(leaf_pid, false);
    return std::nullopt;
}

// INSERCION

void BTree::insert(int32_t key, RID rid) {
    // Árbol vacío: crear raíz hoja
    if (root_page_id_ == INVALID_PAGE) {
        root_page_id_ = create_node(true);
    }

    PageId leaf_pid = find_leaf(key);
    Page*  page     = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    if (!node.is_full()) {
        bm_.unpin_page(leaf_pid, false);
        insert_in_leaf(leaf_pid, key, rid);
    } else {
        bm_.unpin_page(leaf_pid, false);
        split_leaf(leaf_pid, key, rid);
    }
}

void BTree::insert_in_leaf(PageId leaf_pid, int32_t key, RID rid) {
    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    uint32_t n    = node.num_keys();
    int32_t* keys = node.keys();
    RID*     rids = node.rids();

    // Desplazar claves mayores hacia la derecha
    uint32_t i = n;
    while (i > 0 && keys[i-1] > key) {
        keys[i] = keys[i-1];
        rids[i] = rids[i-1];
        i--;
    }
    keys[i] = key;
    rids[i] = rid;
    node.header()->num_keys++;

    bm_.unpin_page(leaf_pid, true);
}

void BTree::split_leaf(PageId leaf_pid, int32_t key, RID rid) {
    // 1. Recopilar todas las claves + la nueva en orden
    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode old_node(page->data_);

    uint32_t n    = old_node.num_keys();
    int32_t* keys = old_node.keys();
    RID*     rids = old_node.rids();

    // Buffer temporal con BTREE_ORDER+1 entradas
    std::vector<int32_t> tmp_keys(n + 1);
    std::vector<RID>     tmp_rids(n + 1);

    uint32_t inserted = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!inserted && keys[i] > key) {
            tmp_keys[i]   = key;
            tmp_rids[i]   = rid;
            inserted = 1;
        }
        tmp_keys[i + inserted] = keys[i];
        tmp_rids[i + inserted] = rids[i];
    }
    if (!inserted) {
        tmp_keys[n] = key;
        tmp_rids[n] = rid;
    }

    // 2. Calcular punto de split: mitad superior va al nuevo nodo
    uint32_t split = (n + 1) / 2;

    // 3. Crear nodo derecho
    PageId parent_id = old_node.header()->parent_id;
    PageId right_pid = create_node(true, parent_id);

    // 4. Llenar nodo izquierdo con la mitad inferior
    old_node.header()->num_keys = split;
    for (uint32_t i = 0; i < split; i++) {
        keys[i] = tmp_keys[i];
        rids[i] = tmp_rids[i];
    }

    // 5. Llenar nodo derecho con la mitad superior
    Page* right_page = bm_.fetch_page(right_pid);
    BTreeNode right_node(right_page->data_);
    right_node.header()->num_keys = (n + 1) - split;

    int32_t* rk = right_node.keys();
    RID*     rr = right_node.rids();
    for (uint32_t i = 0; i < (n + 1) - split; i++) {
        rk[i] = tmp_keys[split + i];
        rr[i] = tmp_rids[split + i];
    }

    // 6. Enlazar hojas: izq → der → antiguo siguiente
    right_node.header()->next_leaf = old_node.header()->next_leaf;
    old_node.header()->next_leaf   = right_pid;

    bm_.unpin_page(right_pid, true);
    bm_.unpin_page(leaf_pid,  true);

    // 7. Propagar la clave mínima del nodo derecho al padre
    insert_in_parent(leaf_pid, rk[0], right_pid);
}

void BTree::insert_in_parent(PageId left_pid, int32_t key, PageId right_pid) {
    // Si left es la raíz, crear nueva raíz
    if (left_pid == root_page_id_) {
        create_new_root(left_pid, key, right_pid);
        return;
    }

    // Buscar el padre
    Page* left_page = bm_.fetch_page(left_pid);
    PageId parent_pid = reinterpret_cast<BTreeNodeHeader*>(
        left_page->data_)->parent_id;
    bm_.unpin_page(left_pid, false);

    Page* parent_page = bm_.fetch_page(parent_pid);
    BTreeNode parent(parent_page->data_);

    if (!parent.is_full()) {
        // Hay espacio: insertar directamente
        uint32_t n   = parent.num_keys();
        int32_t* pks = parent.keys();
        PageId*  pcs = parent.children();

        // Encontrar posición
        uint32_t i = n;
        while (i > 0 && pks[i-1] > key) {
            pks[i]   = pks[i-1];
            pcs[i+1] = pcs[i];
            i--;
        }
        pks[i]   = key;
        pcs[i+1] = right_pid;
        parent.header()->num_keys++;

        // Actualizar parent_id del nodo derecho
        Page* right_page = bm_.fetch_page(right_pid);
        reinterpret_cast<BTreeNodeHeader*>(right_page->data_)->parent_id
            = parent_pid;
        bm_.unpin_page(right_pid, true);
        bm_.unpin_page(parent_pid, true);
    } else {
        // Nodo interno lleno: split del interno
        bm_.unpin_page(parent_pid, false);
        split_internal(parent_pid, key, right_pid);
    }
}

void BTree::split_internal(PageId node_pid, int32_t key, PageId right_child) {
    Page* page = bm_.fetch_page(node_pid);
    BTreeNode node(page->data_);

    uint32_t n   = node.num_keys();
    int32_t* ks  = node.keys();
    PageId*  cs  = node.children();

    // Buffer temporal
    std::vector<int32_t> tmp_keys(n + 1);
    std::vector<PageId>  tmp_children(n + 2);

    // Insertar nueva clave en orden
    uint32_t inserted = 0;
    tmp_children[0] = cs[0];
    for (uint32_t i = 0; i < n; i++) {
        if (!inserted && ks[i] > key) {
            tmp_keys[i]       = key;
            tmp_children[i+1] = right_child;
            inserted = 1;
        }
        tmp_keys[i + inserted]       = ks[i];
        tmp_children[i + inserted+1] = cs[i+1];
    }
    if (!inserted) {
        tmp_keys[n]       = key;
        tmp_children[n+1] = right_child;
    }

    // Split: la clave del medio sube al padre
    uint32_t mid = (n + 1) / 2;
    int32_t  push_up_key = tmp_keys[mid];

    // Nodo izquierdo: claves antes del mid
    node.header()->num_keys = mid;
    for (uint32_t i = 0; i < mid; i++) {
        ks[i] = tmp_keys[i];
        cs[i] = tmp_children[i];
    }
    cs[mid] = tmp_children[mid];

    // Nodo derecho: claves después del mid
    PageId parent_id = node.header()->parent_id;
    bm_.unpin_page(node_pid, true);

    PageId new_pid = create_node(false, parent_id);
    Page*  new_page = bm_.fetch_page(new_pid);
    BTreeNode new_node(new_page->data_);

    uint32_t right_count = n - mid;
    new_node.header()->num_keys = right_count;
    int32_t* nks = new_node.keys();
    PageId*  ncs = new_node.children();

    for (uint32_t i = 0; i < right_count; i++) {
        nks[i] = tmp_keys[mid + 1 + i];
        ncs[i] = tmp_children[mid + 1 + i];
    }
    ncs[right_count] = tmp_children[n + 1];

    bm_.unpin_page(new_pid, true);

    insert_in_parent(node_pid, push_up_key, new_pid);
}

void BTree::create_new_root(PageId left_pid, int32_t key, PageId right_pid) {
    PageId new_root = create_node(false);
    root_page_id_   = new_root;

    Page* page = bm_.fetch_page(new_root);
    BTreeNode root(page->data_);

    root.header()->num_keys = 1;
    root.keys()[0]          = key;
    root.children()[0]      = left_pid;
    root.children()[1]      = right_pid;
    bm_.unpin_page(new_root, true);

    // Actualizar parent_id de ambos hijos
    auto set_parent = [&](PageId pid) {
        Page* p = bm_.fetch_page(pid);
        reinterpret_cast<BTreeNodeHeader*>(p->data_)->parent_id = new_root;
        bm_.unpin_page(pid, true);
    };
    set_parent(left_pid);
    set_parent(right_pid);
}

//ELIMINACION

bool BTree::remove(int32_t key) {
    if (root_page_id_ == INVALID_PAGE) return false;

    PageId leaf_pid = find_leaf(key);
    Page*  page     = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    // Verificar que la clave existe
    uint32_t n    = node.num_keys();
    int32_t* keys = node.keys();
    bool found = false;
    for (uint32_t i = 0; i < n; i++) {
        if (keys[i] == key) { found = true; break; }
    }
    bm_.unpin_page(leaf_pid, false);
    if (!found) return false;

    remove_from_leaf(leaf_pid, key);
    return true;
}

void BTree::remove_from_leaf(PageId leaf_pid, int32_t key) {
    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    uint32_t n    = node.num_keys();
    int32_t* keys = node.keys();
    RID*     rids = node.rids();

    // Encontrar y eliminar la clave desplazando
    uint32_t i = 0;
    while (i < n && keys[i] != key) i++;
    while (i < n - 1) {
        keys[i] = keys[i+1];
        rids[i] = rids[i+1];
        i++;
    }
    node.header()->num_keys--;
    uint32_t min_keys = (BTREE_ORDER) / 2;

    PageId parent_pid = node.header()->parent_id;
    uint32_t new_n    = node.header()->num_keys;
    bm_.unpin_page(leaf_pid, true);

    // Si es la raíz o tiene suficientes claves → listo
    if (parent_pid == INVALID_PAGE || new_n >= min_keys) return;

    // Necesita redistribución o merge
    Page* parent_page = bm_.fetch_page(parent_pid);
    BTreeNode parent(parent_page->data_);

    uint32_t pn  = parent.num_keys();
    PageId*  pcs = parent.children();

    // Encontrar posición del leaf en el padre
    int pos = -1;
    for (uint32_t i = 0; i <= pn; i++) {
        if (pcs[i] == leaf_pid) { pos = i; break; }
    }
    bm_.unpin_page(parent_pid, false);

    if (pos < 0) return;

    // Intentar pedir prestado a hermano
    if (!borrow_from_sibling(leaf_pid, parent_pid, pos)) {
        // No se pudo → merge
        if (pos > 0) {
            merge_leaves(pcs[pos-1], leaf_pid, parent_pid, pos-1);
        } else {
            // Refetch parent para obtener pcs actualizado
            Page* pp = bm_.fetch_page(parent_pid);
            BTreeNode par(pp->data_);
            PageId right = par.children()[1];
            bm_.unpin_page(parent_pid, false);
            merge_leaves(leaf_pid, right, parent_pid, 0);
        }
    }
}

bool BTree::borrow_from_sibling(PageId leaf_pid, PageId parent_pid, int pos) {
    Page* parent_page = bm_.fetch_page(parent_pid);
    BTreeNode parent(parent_page->data_);
    uint32_t pn  = parent.num_keys();
    PageId*  pcs = parent.children();
    int32_t* pks = parent.keys();

    uint32_t min_keys = BTREE_ORDER / 2;
    bool borrowed = false;

    // Intentar hermano derecho
    if ((uint32_t)pos < pn) {
        PageId right_pid = pcs[pos + 1];
        Page*  rp  = bm_.fetch_page(right_pid);
        BTreeNode right(rp->data_);

        if (right.num_keys() > min_keys) {
            // Tomar la primera clave del hermano derecho
            Page*  lp   = bm_.fetch_page(leaf_pid);
            BTreeNode leaf(lp->data_);
            uint32_t ln = leaf.num_keys();

            leaf.keys()[ln] = right.keys()[0];
            leaf.rids()[ln] = right.rids()[0];
            leaf.header()->num_keys++;

            // Desplazar hermano derecho
            uint32_t rn = right.num_keys();
            for (uint32_t i = 0; i < rn - 1; i++) {
                right.keys()[i] = right.keys()[i+1];
                right.rids()[i] = right.rids()[i+1];
            }
            right.header()->num_keys--;

            // Actualizar clave en el padre
            pks[pos] = right.keys()[0];

            bm_.unpin_page(leaf_pid,  true);
            bm_.unpin_page(right_pid, true);
            borrowed = true;
        } else {
            bm_.unpin_page(right_pid, false);
        }
    }

    // Intentar hermano izquierdo
    if (!borrowed && pos > 0) {
        PageId left_pid = pcs[pos - 1];
        Page*  lp  = bm_.fetch_page(left_pid);
        BTreeNode left(lp->data_);

        if (left.num_keys() > min_keys) {
            Page*     cp   = bm_.fetch_page(leaf_pid);
            BTreeNode cur(cp->data_);
            uint32_t  cn   = cur.num_keys();
            uint32_t  ln   = left.num_keys();

            // Hacer espacio al inicio del nodo actual
            for (uint32_t i = cn; i > 0; i--) {
                cur.keys()[i] = cur.keys()[i-1];
                cur.rids()[i] = cur.rids()[i-1];
            }
            cur.keys()[0] = left.keys()[ln-1];
            cur.rids()[0] = left.rids()[ln-1];
            cur.header()->num_keys++;
            left.header()->num_keys--;

            // Actualizar clave en el padre
            pks[pos-1] = cur.keys()[0];

            bm_.unpin_page(leaf_pid, true);
            bm_.unpin_page(left_pid, true);
            borrowed = true;
        } else {
            bm_.unpin_page(left_pid, false);
        }
    }

    bm_.unpin_page(parent_pid, borrowed);
    return borrowed;
}

void BTree::merge_leaves(PageId left_pid, PageId right_pid,PageId parent_pid, int pos){
    // Mover todas las claves del nodo derecho al izquierdo
    Page* lp = bm_.fetch_page(left_pid);
    Page* rp = bm_.fetch_page(right_pid);
    BTreeNode left(lp->data_);
    BTreeNode right(rp->data_);

    uint32_t ln = left.num_keys();
    uint32_t rn = right.num_keys();

    for (uint32_t i = 0; i < rn; i++) {
        left.keys()[ln + i] = right.keys()[i];
        left.rids()[ln + i] = right.rids()[i];
    }
    left.header()->num_keys += rn;
    left.header()->next_leaf = right.header()->next_leaf;

    bm_.unpin_page(left_pid,  true);
    bm_.unpin_page(right_pid, true);

    // Eliminar la clave del padre que separaba estos dos nodos
    Page* pp = bm_.fetch_page(parent_pid);
    BTreeNode parent(pp->data_);

    uint32_t pn  = parent.num_keys();
    int32_t* pks = parent.keys();
    PageId*  pcs = parent.children();

    for (int i = pos; i < (int)pn - 1; i++) {
        pks[i]   = pks[i+1];
        pcs[i+1] = pcs[i+2];
    }
    parent.header()->num_keys--;

    // Si la raíz quedó vacía, el hijo izquierdo es la nueva raíz
    if (pn - 1 == 0 && parent_pid == root_page_id_) {
        root_page_id_ = left_pid;
    }

    bm_.unpin_page(parent_pid, true);
}

//PRINT

void BTree::print_tree() {
    if (root_page_id_ == INVALID_PAGE) {
        std::cout << "[árbol vacío]\n";
        return;
    }
    std::queue<PageId> q;
    q.push(root_page_id_);
    while (!q.empty()) {
        PageId pid = q.front(); q.pop();
        Page* page = bm_.fetch_page(pid);
        BTreeNode node(page->data_);
        node.print();
        if (!node.is_leaf()) {
            uint32_t n = node.num_keys();
            for (uint32_t i = 0; i <= n; i++) {
                if (node.children()[i] != INVALID_PAGE)
                    q.push(node.children()[i]);
            }
        }
        bm_.unpin_page(pid, false);
    }
    std::cout << "\n";
}
#include "btree.h"
#include <iostream>

BTree::BTree(BufferManager& bm, PageId root_page_id)
    : bm_(bm), root_page_id_(root_page_id)
{}

PageId BTree::create_node(bool is_leaf, PageId parent_id) {
    PageId pid;
    Page* page = bm_.new_page(pid);

    // Construir vista del nodo sobre la página
    BTreeNode node(page->data_);
    node.init(pid, is_leaf, parent_id);

    // Marcar dirty porque lo inicializamos
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

        // Nodo interno: encontrar el puntero correcto
        // Busca el primer i tal que key < keys[i]
        uint32_t n = node.num_keys();
        int32_t* keys = node.keys();
        PageId*  children = node.children();

        uint32_t i = 0;
        while (i < n && key >= keys[i]) i++;

        PageId next = children[i];
        bm_.unpin_page(current, false);
        current = next;
    }
}

std::optional<RID> BTree::search(int32_t key) {
    if (root_page_id_ == INVALID_PAGE) return std::nullopt;

    // 1. Bajar hasta la hoja
    PageId leaf_pid = find_leaf(key);
    if (leaf_pid == INVALID_PAGE) return std::nullopt;

    // 2. Buscar la clave en la hoja (búsqueda lineal)
    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    uint32_t n = node.num_keys();
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
    return std::nullopt;  // no encontrada
}

void BTree::insert(int32_t key, RID rid) {
    if (root_page_id_ == INVALID_PAGE) {
        // Árbol vacío: crear raíz que es hoja
        root_page_id_ = create_node(true);
    }

    PageId leaf_pid = find_leaf(key);
    Page* page = bm_.fetch_page(leaf_pid);
    BTreeNode node(page->data_);

    if (node.is_full()) {
        bm_.unpin_page(leaf_pid, false);
        std::cerr << "Nodo lleno: split no implementado aún (semana 11)\n";
        return;
    }

    // Insertar en orden
    uint32_t n = node.num_keys();
    int32_t* keys = node.keys();
    RID*     rids = node.rids();

    uint32_t i = n;
    while (i > 0 && keys[i-1] > key) {
        keys[i] = keys[i-1];
        rids[i] = rids[i-1];
        i--;
    }
    keys[i] = key;
    rids[i] = rid;
    node.header()->num_keys++;

    bm_.unpin_page(leaf_pid, true);  // dirty
}

void BTree::print_tree() {
    if (root_page_id_ == INVALID_PAGE) {
        std::cout << "[árbol vacío]\n";
        return;
    }

    // BFS para imprimir nivel por nivel
    std::vector<PageId> current_level = { root_page_id_ };

    while (!current_level.empty()) {
        std::vector<PageId> next_level;

        for (PageId pid : current_level) {
            Page* page = bm_.fetch_page(pid);
            BTreeNode node(page->data_);
            node.print();

            if (!node.is_leaf()) {
                uint32_t n = node.num_keys();
                PageId* children = node.children();
                for (uint32_t i = 0; i <= n; i++) {
                    if (children[i] != INVALID_PAGE)
                        next_level.push_back(children[i]);
                }
            }
            bm_.unpin_page(pid, false);
        }
        std::cout << "\n";
        current_level = next_level;
    }
}
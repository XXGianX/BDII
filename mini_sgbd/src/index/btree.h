#pragma once
#include "btree_node.h"
#include "../buffer/buffer_manager.h"
#include <optional>

class BTree {
public:
    // El B+ Tree usa el BufferManager para leer/escribir nodos
    // root_page_id: página donde está la raíz (INVALID_PAGE si árbol vacío)
    BTree(BufferManager& bm, PageId root_page_id = INVALID_PAGE);

    // Busca una clave y devuelve su RID si existe
    std::optional<RID> search(int32_t key);

    // Inserta una clave con su RID
    void insert(int32_t key, RID rid);

    PageId root_page_id() const { return root_page_id_; }

    // Muestra el árbol completo (debug)
    void print_tree();

private:
    BufferManager& bm_;
    PageId         root_page_id_;

    // Busca la hoja donde debería estar la clave
    PageId find_leaf(int32_t key);

    // Crea un nodo nuevo via BufferManager
    PageId create_node(bool is_leaf, PageId parent_id = INVALID_PAGE);
};
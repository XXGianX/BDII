#pragma once
#include "../common/config.h"
#include "../common/types.h"
#include <cstring>

// Orden del árbol: máximo de claves por nodo
// Con PAGE_SIZE=4096 y claves int32: caben ~500, pero usamos 4 para probar splits
constexpr uint32_t BTREE_ORDER = 4;  // max claves por nodo

/*
  Layout de un nodo B+ dentro de una página de 4096 bytes:
  ┌──────────────────────────────────────────────┐
  │ BTreeNodeHeader (24 bytes)                   │
  │   is_leaf    : 1 byte                        │
  │   num_keys   : 4 bytes                       │
  │   page_id    : 4 bytes  (este nodo)          │
  │   parent_id  : 4 bytes                       │
  │   next_leaf  : 4 bytes  (solo en hojas)      │
  │   reserved   : 7 bytes                       │
  ├──────────────────────────────────────────────┤
  │ Claves: int32_t[BTREE_ORDER]                 │
  ├──────────────────────────────────────────────┤
  │ Punteros/RIDs:                               │
  │   Nodo interno: PageId[BTREE_ORDER + 1]      │
  │   Nodo hoja:    RID[BTREE_ORDER]             │
  └──────────────────────────────────────────────┘
*/

struct BTreeNodeHeader {
    uint8_t  is_leaf    = 0;            // 1 = hoja, 0 = interno
    uint32_t num_keys   = 0;            // cuántas claves activas
    PageId   page_id    = INVALID_PAGE; // id de esta página
    PageId   parent_id  = INVALID_PAGE; // id del nodo padre
    PageId   next_leaf  = INVALID_PAGE; // siguiente hoja (solo hojas)
    uint8_t  reserved[7] = {};
};

// Vista sobre los datos crudos de una página
// No copia datos — opera directamente sobre page.data_
class BTreeNode {
public:
    // Construye una vista sobre el buffer crudo de la página
    explicit BTreeNode(char* data) : data_(data) {}

    BTreeNodeHeader* header() {
        return reinterpret_cast<BTreeNodeHeader*>(data_);
    }

    // Array de claves
    int32_t* keys() {
        return reinterpret_cast<int32_t*>(
            data_ + sizeof(BTreeNodeHeader)
        );
    }

    // Punteros a hijos (nodos internos): BTREE_ORDER + 1 punteros
    PageId* children() {
        return reinterpret_cast<PageId*>(
            data_ + sizeof(BTreeNodeHeader)
                  + sizeof(int32_t) * BTREE_ORDER
        );
    }

    // RIDs para hojas: uno por clave
    RID* rids() {
        return reinterpret_cast<RID*>(
            data_ + sizeof(BTreeNodeHeader)
                  + sizeof(int32_t) * BTREE_ORDER
        );
    }

    bool is_leaf()    const { return reinterpret_cast<const BTreeNodeHeader*>(data_)->is_leaf; }
    bool is_full()    const { return reinterpret_cast<const BTreeNodeHeader*>(data_)->num_keys >= BTREE_ORDER; }
    uint32_t num_keys() const { return reinterpret_cast<const BTreeNodeHeader*>(data_)->num_keys; }

    // Inicializa el nodo limpio
    void init(PageId page_id, bool leaf, PageId parent_id = INVALID_PAGE) {
        memset(data_, 0, PAGE_SIZE);
        BTreeNodeHeader* h = header();
        h->is_leaf    = leaf ? 1 : 0;
        h->num_keys   = 0;
        h->page_id    = page_id;
        h->parent_id  = parent_id;
        h->next_leaf  = INVALID_PAGE;
    }

    // Muestra el contenido del nodo (para debug)
    void print() const;

private:
    char* data_;
};
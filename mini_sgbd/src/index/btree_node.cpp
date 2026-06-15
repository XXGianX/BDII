#include "btree_node.h"
#include <iostream>

void BTreeNode::print() const {
    const BTreeNodeHeader* h = reinterpret_cast<const BTreeNodeHeader*>(data_);
    std::cout << "[" << (h->is_leaf ? "HOJA" : "INTERNO")
              << " page=" << h->page_id
              << " parent=" << h->parent_id
              << " keys=" << h->num_keys << "] ";

    const int32_t* k = reinterpret_cast<const int32_t*>(
        data_ + sizeof(BTreeNodeHeader)
    );

    std::cout << "claves: ";
    for (uint32_t i = 0; i < h->num_keys; i++) {
        std::cout << k[i] << " ";
    }

    if (h->is_leaf) {
        const RID* r = reinterpret_cast<const RID*>(
            data_ + sizeof(BTreeNodeHeader)
                  + sizeof(int32_t) * BTREE_ORDER
        );
        std::cout << "| RIDs: ";
        for (uint32_t i = 0; i < h->num_keys; i++) {
            std::cout << "(" << r[i].page_id << "," << r[i].slot_id << ") ";
        }
        std::cout << "| next=" << h->next_leaf;
    } else {
        const PageId* c = reinterpret_cast<const PageId*>(
            data_ + sizeof(BTreeNodeHeader)
                  + sizeof(int32_t) * BTREE_ORDER
        );
        std::cout << "| hijos: ";
        for (uint32_t i = 0; i <= h->num_keys; i++) {
            std::cout << c[i] << " ";
        }
    }
    std::cout << "\n";
}
#pragma once
#include "btree_node.h"
#include "../buffer/buffer_manager.h"
#include <optional>
#include <vector>

class BTree {
public:
    BTree(BufferManager& bm, PageId root_page_id = INVALID_PAGE);

    std::optional<RID> search(int32_t key);
    void insert(int32_t key, RID rid);

    //eliminación
    bool remove(int32_t key);

    PageId root_page_id() const { return root_page_id_; }
    void print_tree();

private:
    BufferManager& bm_;
    PageId         root_page_id_;

    PageId find_leaf(int32_t key);
    PageId create_node(bool is_leaf, PageId parent_id = INVALID_PAGE);

    //splitting
    void insert_in_leaf(PageId leaf_pid, int32_t key, RID rid);
    void split_leaf(PageId leaf_pid, int32_t key, RID rid);
    void split_internal(PageId node_pid, int32_t key, PageId right_pid);
    void insert_in_parent(PageId left_pid, int32_t key, PageId right_pid);
    void create_new_root(PageId left_pid, int32_t key, PageId right_pid);

    // eliminación
    void remove_from_leaf(PageId leaf_pid, int32_t key);
    bool borrow_from_sibling(PageId leaf_pid, PageId parent_pid, int pos);
    void merge_leaves(PageId left_pid, PageId right_pid,PageId parent_pid, int pos);
};
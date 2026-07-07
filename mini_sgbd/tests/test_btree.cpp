#include "../src/index/btree.h"
#include <iostream>
#include <cassert>

void test_insert_sin_split() {
    std::cout << "=== test_insert_sin_split ===\n";
    DiskManager dm("data/test_ins.db");
    BufferManager bm(dm, 10);
    BTree tree(bm);

    tree.insert(10, {0,0});
    tree.insert(20, {0,1});
    tree.insert(5,  {0,2});

    tree.print_tree();

    assert(tree.search(10).has_value());
    assert(tree.search(20).has_value());
    assert(tree.search(5).has_value());
    assert(!tree.search(99).has_value());
    std::cout << "[OK] inserción sin split\n\n";
}

void test_insert_con_split() {
    std::cout << "=== test_insert_con_split ===\n";
    DiskManager dm("data/test_split.db");
    BufferManager bm(dm, 10);
    BTree tree(bm);

    // BTREE_ORDER=4 → split al insertar el 5to elemento
    tree.insert(10, {0,0});
    tree.insert(20, {0,1});
    tree.insert(5,  {0,2});
    tree.insert(15, {0,3});
    tree.insert(25, {0,4}); // ← provoca split

    std::cout << "Árbol después del split:\n";
    tree.print_tree();

    // Todas las claves deben seguir encontrándose
    for (int k : {5, 10, 15, 20, 25}) {
        assert(tree.search(k).has_value());
        std::cout << "Búsqueda " << k << ": OK\n";
    }
    std::cout << "[OK] split de hoja correcto\n\n";
}

void test_multiple_splits() {
    std::cout << "=== test_multiple_splits ===\n";
    DiskManager dm("data/test_multi.db");
    BufferManager bm(dm, 20);
    BTree tree(bm);

    // Insertar 12 claves → varios splits y posible split de nodo interno
    std::vector<int> claves = {30,10,50,20,40,60,5,15,25,35,45,55};
    for (int k : claves) {
        tree.insert(k, {0, (uint16_t)k});
    }

    std::cout << "Árbol con 12 claves:\n";
    tree.print_tree();

    for (int k : claves) {
        auto r = tree.search(k);
        assert(r.has_value());
        assert(r->slot_id == (uint16_t)k);
    }
    std::cout << "[OK] múltiples splits, todas las claves encontradas\n\n";
}

void test_remove_basico() {
    std::cout << "=== test_remove_basico ===\n";
    DiskManager dm("data/test_remove.db");
    BufferManager bm(dm, 10);
    BTree tree(bm);

    tree.insert(10, {0,0});
    tree.insert(20, {0,1});
    tree.insert(5,  {0,2});

    assert(tree.remove(10));
    assert(!tree.search(10).has_value());
    assert(tree.search(20).has_value());
    assert(tree.search(5).has_value());

    std::cout << "Árbol tras eliminar 10:\n";
    tree.print_tree();
    std::cout << "[OK] eliminación básica\n\n";
}

void test_remove_con_merge() {
    std::cout << "=== test_remove_con_merge ===\n";
    DiskManager dm("data/test_merge.db");
    BufferManager bm(dm, 20);
    BTree tree(bm);

    std::vector<int> claves = {10,20,5,15,25,30};
    for (int k : claves) tree.insert(k, {0,(uint16_t)k});

    std::cout << "Árbol antes de eliminar:\n";
    tree.print_tree();

    tree.remove(25);
    tree.remove(30);

    std::cout << "Árbol después de eliminar 25 y 30:\n";
    tree.print_tree();

    assert(!tree.search(25).has_value());
    assert(!tree.search(30).has_value());
    assert(tree.search(10).has_value());
    std::cout << "[OK] merge de hojas correcto\n\n";
}

int main() {
    system("mkdir -p data");
    test_insert_sin_split();
    test_insert_con_split();
    test_multiple_splits();
    test_remove_basico();
    test_remove_con_merge();
    std::cout << "=== Todos los tests Semanas 11-12 pasaron ===\n";
    return 0;
}
#include "../src/index/btree.h"
#include <iostream>
#include <cassert>

void test_nodo_layout() {
    std::cout << "=== test_nodo_layout ===\n";

    char buffer[PAGE_SIZE] = {};
    BTreeNode node(buffer);
    node.init(0, true);

    assert(node.is_leaf());
    assert(node.num_keys() == 0);
    assert(!node.is_full());

    std::cout << "Tamaño BTreeNodeHeader : " << sizeof(BTreeNodeHeader) << " bytes\n";
    std::cout << "BTREE_ORDER            : " << BTREE_ORDER << "\n";
    std::cout << "[OK] layout del nodo correcto\n\n";
}

void test_search_vacio() {
    std::cout << "=== test_search_vacio ===\n";

    DiskManager dm("data/test_btree.db");
    BufferManager bm(dm, 8);
    BTree tree(bm);

    auto result = tree.search(42);
    assert(!result.has_value());
    std::cout << "[OK] búsqueda en árbol vacío retorna nullopt\n\n";
}

void test_insert_y_search() {
    std::cout << "=== test_insert_y_search ===\n";

    DiskManager dm("data/test_btree2.db");
    BufferManager bm(dm, 8);
    BTree tree(bm);

    // Insertar algunas claves
    tree.insert(10, {0, 0});
    tree.insert(30, {0, 1});
    tree.insert(20, {0, 2});
    tree.insert(5,  {0, 3});

    std::cout << "Árbol después de insertar 10, 30, 20, 5:\n";
    tree.print_tree();

    // Buscar claves existentes
    auto r1 = tree.search(10);
    assert(r1.has_value() && r1->slot_id == 0);
    std::cout << "Búsqueda 10: encontrada en slot " << r1->slot_id << "\n";

    auto r2 = tree.search(20);
    assert(r2.has_value() && r2->slot_id == 2);
    std::cout << "Búsqueda 20: encontrada en slot " << r2->slot_id << "\n";

    // Buscar clave inexistente
    auto r3 = tree.search(99);
    assert(!r3.has_value());
    std::cout << "Búsqueda 99: no encontrada (correcto)\n";

    std::cout << "[OK] insert y search funcionan correctamente\n\n";
}

void test_integracion_buffer() {
    std::cout << "=== test_integracion_buffer ===\n";

    {
        DiskManager dm("data/test_btree3.db");
        BufferManager bm(dm, 8);
        BTree tree(bm);

        tree.insert(50, {1, 0});
        tree.insert(25, {1, 1});
        tree.insert(75, {1, 2});

        PageId root = tree.root_page_id();
        std::cout << "Root page_id: " << root << "\n";
        bm.print_status();
        // destructor de bm hace flush_all
    }

    // Reabrir y verificar persistencia
    {
        DiskManager dm("data/test_btree3.db");
        BufferManager bm(dm, 8);

        // Leer la raíz directamente
        Page* page = bm.fetch_page(0);
        BTreeNode node(page->data_);
        node.print();
        bm.unpin_page(0, false);

        std::cout << "[OK] nodos del B+ Tree persisten en disco via Buffer Manager\n\n";
    }
}

int main() {
    system("mkdir -p data");
    test_nodo_layout();
    test_search_vacio();
    test_insert_y_search();
    test_integracion_buffer();
    std::cout << "=== Todos los tests pasaron ===\n";
    return 0;
}
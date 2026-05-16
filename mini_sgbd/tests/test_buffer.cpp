#include "../src/buffer/buffer_manager.h"
#include "../src/storage/slot_directory.h"
#include <iostream>
#include <cassert>
#include <cstring>

void test_buffer_pool_estructuras() {
    std::cout << "=== test_buffer_pool_estructuras ===\n";

    DiskManager dm("data/test_buffer.db");
    BufferManager bm(dm, 4);  // 4 frames en RAM

    // Al inicio todos los frames deben estar vacíos
    bm.print_status();

    PageId pid0, pid1, pid2;
    Page* p0 = bm.new_page(pid0);
    Page* p1 = bm.new_page(pid1);
    Page* p2 = bm.new_page(pid2);

    assert(pid0 == 0 && pid1 == 1 && pid2 == 2);
    std::cout << "Páginas creadas: " << pid0 << ", " << pid1 << ", " << pid2 << "\n";

    // Ver cómo quedó la Page Table
    bm.print_status();

    bm.unpin_page(pid0, false);
    bm.unpin_page(pid1, false);
    bm.unpin_page(pid2, false);

    std::cout << "[OK] Buffer Pool y Page Table funcionan\n";
}

void test_page_table_lookup() {
    std::cout << "\n=== test_page_table_lookup ===\n";

    DiskManager dm("data/test_buffer.db");
    BufferManager bm(dm, 4);

    // Insertar un registro en la página 0 via buffer
    PageId pid;
    Page* p = bm.new_page(pid);

    const char* rec = "registro via buffer manager";
    int sid = SlotDirectory::insert(*p, rec, strlen(rec));
    bm.unpin_page(pid, true); // dirty = true

    // Volver a buscar la misma página — debe estar en la Page Table (HIT)
    Page* p2 = bm.fetch_page(pid);
    uint16_t len;
    const char* r = SlotDirectory::get(*p2, sid, len);
    assert(r != nullptr);
    std::string result(r, len);
    assert(result == "registro via buffer manager");
    std::cout << "Registro recuperado desde buffer: " << result << "\n";
    bm.unpin_page(pid, false);

    std::cout << "[OK] Page Table resuelve correctamente page_id → frame\n";
}

void test_pin_count() {
    std::cout << "\n=== test_pin_count ===\n";

    DiskManager dm("data/test_buffer.db");
    BufferManager bm(dm, 4);

    PageId pid;
    Page* p = bm.new_page(pid); // pin_count = 1

    // Fetch adicionales incrementan el pin_count
    bm.fetch_page(pid);          // pin_count = 2
    bm.fetch_page(pid);          // pin_count = 3

    bm.print_status();

    bm.unpin_page(pid, false);   // pin_count = 2
    bm.unpin_page(pid, false);   // pin_count = 1
    bm.unpin_page(pid, false);   // pin_count = 0

    bm.print_status();
    std::cout << "[OK] pin_count sube y baja correctamente\n";
}

void test_dirty_persiste() {
    std::cout << "\n=== test_dirty_persiste ===\n";
    {
        DiskManager dm("data/test_dirty.db");
        BufferManager bm(dm, 2);

        PageId pid;
        Page* p = bm.new_page(pid);
        const char* msg = "persistido via buffer semana 6";
        SlotDirectory::insert(*p, msg, strlen(msg));
        bm.unpin_page(pid, true); // dirty
        // destructor llama flush_all()
    }
    {
        DiskManager dm("data/test_dirty.db");
        BufferManager bm(dm, 2);
        Page* p = bm.fetch_page(0);
        uint16_t len;
        const char* r = SlotDirectory::get(*p, 0, len);
        assert(r != nullptr);
        std::string result(r, len);
        assert(result == "persistido via buffer semana 6");
        std::cout << "Dato recuperado del disco: " << result << "\n";
        bm.unpin_page(0, false);
        std::cout << "[OK] dirty page persistida correctamente\n";
    }
}

int main() {
    system("mkdir -p data");
    test_buffer_pool_estructuras();
    test_page_table_lookup();
    test_pin_count();
    test_dirty_persiste();
    std::cout << "\n=== Todos los tests pasaron ===\n";
    return 0;
}
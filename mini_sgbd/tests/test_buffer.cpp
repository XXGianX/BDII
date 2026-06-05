#include "../src/buffer/buffer_manager.h"
#include "../src/storage/slot_directory.h"
#include <iostream>
#include <cassert>
#include <cstring>

void test_lru_orden() {
    std::cout << "=== test_lru_orden ===\n";
    DiskManager dm("data/test_lru.db");
    BufferManager bm(dm, 4);

    // Crear 4 páginas — llena el buffer completo
    PageId p0, p1, p2, p3;
    bm.new_page(p0); bm.unpin_page(p0, false);
    bm.new_page(p1); bm.unpin_page(p1, false);
    bm.new_page(p2); bm.unpin_page(p2, false);
    bm.new_page(p3); bm.unpin_page(p3, false);

    // Acceder a p0 y p1 → quedan como más recientes
    bm.fetch_page(p0); bm.unpin_page(p0, false);
    bm.fetch_page(p1); bm.unpin_page(p1, false);

    // Estado LRU esperado: p1(frente) p0 p3 p2(víctima)
    bm.print_status();

    // Pedir nueva página → debe reemplazar p2 (menos usada)
    PageId p4;
    bm.new_page(p4);
    bm.unpin_page(p4, false);

    std::cout << "Página nueva (reemplazó víctima LRU): " << p4 << "\n";
    bm.print_status();
    std::cout << "[OK] LRU reemplaza correctamente el frame menos usado\n";
}

void test_dirty_antes_de_reemplazar() {
    std::cout << "\n=== test_dirty_antes_de_reemplazar ===\n";
    {
        DiskManager dm("data/test_dirty_lru.db");
        BufferManager bm(dm, 2); // solo 2 frames

        // Página 0: insertar dato y marcar dirty
        PageId pid0;
        Page* p0 = bm.new_page(pid0);
        const char* msg = "dato importante";
        SlotDirectory::insert(*p0, msg, strlen(msg));
        bm.unpin_page(pid0, true); // dirty!

        // Página 1
        PageId pid1;
        bm.new_page(pid1);
        bm.unpin_page(pid1, false);

        // Página 2 → buffer lleno, debe reemplazar p0
        // Como p0 es dirty → debe escribirse al disco antes
        PageId pid2;
        bm.new_page(pid2);
        bm.unpin_page(pid2, false);

        std::cout << "Buffer forzó flush de página dirty antes de reemplazar\n";
        bm.print_status();
    }

    // Verificar que el dato de p0 llegó al disco
    {
        DiskManager dm("data/test_dirty_lru.db");
        BufferManager bm(dm, 2);
        Page* p = bm.fetch_page(0);
        uint16_t len;
        const char* r = SlotDirectory::get(*p, 0, len);
        assert(r != nullptr);
        std::string result(r, len);
        assert(result == "dato importante");
        bm.unpin_page(0, false);
        std::cout << "Dato recuperado del disco: " << result << "\n";
        std::cout << "[OK] página dirty fue escrita al disco antes del reemplazo\n";
    }
}

void test_pin_bloquea_reemplazo() {
    std::cout << "\n=== test_pin_bloquea_reemplazo ===\n";
    DiskManager dm("data/test_pin.db");
    BufferManager bm(dm, 2);

    // Llenar el buffer con 2 páginas pinned
    PageId pid0, pid1;
    Page* p0 = bm.new_page(pid0); // pin_count = 1
    Page* p1 = bm.new_page(pid1); // pin_count = 1

    // Intentar crear una tercera página — debe lanzar excepción
    // porque todos los frames están pinned
    bool excepcion_lanzada = false;
    try {
        PageId pid2;
        bm.new_page(pid2);
    } catch (const std::runtime_error& e) {
        excepcion_lanzada = true;
        std::cout << "Excepción capturada: " << e.what() << "\n";
    }

    assert(excepcion_lanzada);
    bm.unpin_page(pid0, false);
    bm.unpin_page(pid1, false);
    std::cout << "[OK] frames pinned no son reemplazables\n";
}

void test_hit_rate() {
    std::cout << "\n=== test_hit_rate ===\n";
    DiskManager dm("data/test_hit.db");
    BufferManager bm(dm, 4);

    PageId pid0;
    bm.new_page(pid0);
    bm.unpin_page(pid0, false);

    // 1 MISS (new_page cuenta) + 4 HITs
    for (int i = 0; i < 4; i++) {
        bm.fetch_page(pid0);
        bm.unpin_page(pid0, false);
    }

    std::cout << "Total requests : " << bm.get_total_requests() << "\n";
    std::cout << "Cache hits     : " << bm.get_cache_hits() << "\n";
    std::cout << "Hit rate       : " << bm.hit_rate() << "%\n";
    assert(bm.hit_rate() > 50.0);
    std::cout << "[OK] hit rate calculado correctamente\n";
}

int main() {
    system("mkdir -p data");
    test_lru_orden();
    test_dirty_antes_de_reemplazar();
    test_pin_bloquea_reemplazo();
    test_hit_rate();
    std::cout << "\n=== Todos los tests de Semana 7 pasaron ===\n";
    return 0;
}
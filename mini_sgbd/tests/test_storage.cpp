#include "../src/storage/disk_manager.h"
#include "../src/storage/slot_directory.h"
#include <iostream>
#include <cstring>
#include <cassert>

void test_page_layout() {
    Page page;
    page.header()->page_id = 0;

    std::cout << "=== test_page_layout ===\n";
    std::cout << "Tamaño PageHeader  : " << sizeof(PageHeader) << " bytes\n";
    std::cout << "Tamaño SlotEntry   : " << sizeof(SlotEntry)  << " bytes\n";
    std::cout << "Espacio libre init : " << page.free_space()  << " bytes\n";
    assert(page.free_space() == PAGE_SIZE - sizeof(PageHeader));
    std::cout << "[OK] layout correcto\n\n";
}

void test_insert_and_get() {
    Page page;
    page.header()->page_id = 1;

    std::cout << "=== test_insert_and_get ===\n";

    const char* rec0 = "Estudiante:Ana,Nota:18";
    const char* rec1 = "Estudiante:Luis,Nota:15";
    const char* rec2 = "Estudiante:Maria,Nota:20";

    int s0 = SlotDirectory::insert(page, rec0, strlen(rec0));
    int s1 = SlotDirectory::insert(page, rec1, strlen(rec1));
    int s2 = SlotDirectory::insert(page, rec2, strlen(rec2));

    assert(s0 == 0 && s1 == 1 && s2 == 2);
    std::cout << "Slots asignados: " << s0 << ", " << s1 << ", " << s2 << "\n";

    uint16_t len;
    const char* r = SlotDirectory::get(page, 1, len);
    assert(r != nullptr);
    std::string result(r, len);
    assert(result == rec1);
    std::cout << "Registro slot 1: " << result << "\n";
    std::cout << "[OK] insert y get\n\n";
}

void test_delete_and_recycle() {
    Page page;
    page.header()->page_id = 2;

    std::cout << "=== test_delete_and_recycle ===\n";

    const char* rec = "Registro:A";
    int s0 = SlotDirectory::insert(page, rec, strlen(rec));
    int s1 = SlotDirectory::insert(page, "Registro:B", 10);
    (void)s1;

    assert(SlotDirectory::remove(page, s0));

    uint16_t len;
    assert(SlotDirectory::get(page, s0, len) == nullptr);
    std::cout << "Slot 0 eliminado correctamente\n";

    int s2 = SlotDirectory::insert(page, "Nuevo:X", 7);
    assert(s2 == 0);
    std::cout << "Slot reciclado: " << s2 << "\n";
    std::cout << "[OK] delete y recycle\n\n";
}

void test_persist_and_reload() {
    std::cout << "=== test_persist_and_reload ===\n";

    const char* msg = "Registro persistido correctamente";
    uint16_t msg_len = strlen(msg);
    SlotId written_slot;

    {
        DiskManager dm("data/test_persist.db");
        Page page;
        page.header()->page_id = 0;
        int sid = SlotDirectory::insert(page, msg, msg_len);
        assert(sid >= 0);
        written_slot = sid;
        dm.write_page(0, page.data_);
        std::cout << "Página escrita en disco\n";
    }

    {
        DiskManager dm("data/test_persist.db");
        Page page;
        dm.read_page(0, page.data_);

        uint16_t len;
        const char* r = SlotDirectory::get(page, written_slot, len);
        assert(r != nullptr);
        std::string result(r, len);
        assert(result == msg);
        std::cout << "Registro recuperado: " << result << "\n";
        std::cout << "[OK] persistencia y recarga\n\n";
    }
}

int main() {
    system("mkdir -p data");
    test_page_layout();
    test_insert_and_get();
    test_delete_and_recycle();
    test_persist_and_reload();
    std::cout << "=== Todos los tests pasaron ===\n";
    return 0;
}
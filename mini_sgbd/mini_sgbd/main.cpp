#include <iostream>
#include "src/storage/disk_manager.h"
#include "src/storage/slot_directory.h"

int main() {
    std::cout << "=== Mini SGBD - BDII 2026 ===\n";

    // Crear disco y una página de prueba
    DiskManager dm("data/main.db");

    Page page;
    page.header()->page_id = 0;

    const char* rec = "Registro de prueba desde main";
    int sid = SlotDirectory::insert(page, rec, strlen(rec));
    dm.write_page(0, page.data_);

    std::cout << "Registro insertado en slot: " << sid << "\n";
    std::cout << "Espacio libre restante: " << page.free_space() << " bytes\n";
    return 0;
}
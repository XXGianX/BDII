// tests/test_storage.cpp
#include "../src/storage/disk_manager.h"
#include <iostream>
#include <cstring>
#include <cassert>

int main() {
    DiskManager dm("data/test.db");

    // --- Prueba write_page / read_page ---
    char write_buf[PAGE_SIZE];
    char read_buf[PAGE_SIZE];

    memset(write_buf, 0, PAGE_SIZE);
    const char* msg = "Hola Mini-SGBD, pagina 0!";
    memcpy(write_buf, msg, strlen(msg));

    assert(dm.write_page(0, write_buf) && "write_page falló");
    assert(dm.read_page(0, read_buf)   && "read_page falló");
    assert(memcmp(write_buf, read_buf, PAGE_SIZE) == 0 && "datos no coinciden");

    std::cout << "[OK] write_page / read_page\n";
    std::cout << "[OK] Contenido: " << read_buf << "\n";

    // --- Prueba write_atomic ---
    const char* payload = "escritura atomica con fsync";
    assert(dm.write_atomic(payload, strlen(payload)) && "write_atomic falló");
    std::cout << "[OK] write_atomic completada\n";

    std::cout << "Total de páginas: " << dm.get_num_pages() << "\n";
    return 0;
}
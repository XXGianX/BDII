// src/storage/disk_manager.h
#pragma once
#include <string>
#include <cstdint>

constexpr size_t PAGE_SIZE = 4096; // 4 KB — puedes cambiarlo a 8192

class DiskManager {
public:
    explicit DiskManager(const std::string& db_filename);
    ~DiskManager();

    // Lee la página con id `page_id` y copia los bytes en `page_data`
    bool read_page(uint32_t page_id, char* page_data);

    // Escribe `page_data` en la posición correspondiente a `page_id`
    bool write_page(uint32_t page_id, const char* page_data);

    // Escribe de forma atómica un bloque completo de datos al archivo
    bool write_atomic(const char* data, size_t size);

    uint32_t get_num_pages() const { return num_pages_; }

private:
    std::string filename_;
    int fd_;            // file descriptor POSIX
    uint32_t num_pages_;
};
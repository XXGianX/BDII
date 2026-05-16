#pragma once
#include <string>
#include <cstdint>
#include "../common/config.h"  // PAGE_SIZE viene de aquí, no se redefine

class DiskManager {
public:
    explicit DiskManager(const std::string& db_filename);
    ~DiskManager();

    bool read_page(uint32_t page_id, char* page_data);
    bool write_page(uint32_t page_id, const char* page_data);
    bool write_atomic(const char* data, size_t size);

    uint32_t get_num_pages() const { return num_pages_; }

private:
    std::string filename_;
    int fd_;
    uint32_t num_pages_;
};
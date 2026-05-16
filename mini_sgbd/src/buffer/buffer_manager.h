#pragma once
#include "../common/config.h"
#include "../common/types.h"
#include "../storage/disk_manager.h"
#include "../storage/page.h"
#include <unordered_map>
#include <vector>

// Metadatos de cada frame en el Buffer Pool
struct FrameDescriptor {
    PageId page_id   = INVALID_PAGE; // qué página ocupa este frame
    bool   is_dirty  = false;        // fue modificada?
    int    pin_count = 0;            // cuántos módulos la están usando
};

class BufferManager {
public:
    BufferManager(DiskManager& disk_manager, size_t num_frames);
    ~BufferManager();

    // Trae una página al buffer y la retorna
    Page* fetch_page(PageId page_id);

    // Libera el uso de una página
    void unpin_page(PageId page_id, bool is_dirty);

    // Crea una página nueva en disco y la trae al buffer
    Page* new_page(PageId& out_page_id);

    // Escribe todas las páginas dirty al disco
    void flush_all();

    // Muestra el estado actual del buffer (para debug)
    void print_status() const;

private:
    DiskManager&                        disk_;
    size_t                              num_frames_;

    // El Buffer Pool: arreglo de frames en RAM
    std::vector<Page>                   frames_;

    // Descriptor de cada frame
    std::vector<FrameDescriptor>        descriptors_;

    // Page Table: página → frame
    std::unordered_map<PageId, FrameId> page_table_;

    // Busca un frame libre 
    FrameId find_free_frame();
};
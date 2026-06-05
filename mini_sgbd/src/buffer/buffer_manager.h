#pragma once
#include "../common/config.h"
#include "../common/types.h"
#include "../storage/disk_manager.h"
#include "../storage/page.h"
#include <unordered_map>
#include <vector>
#include <list>

struct FrameDescriptor {
    PageId page_id   = INVALID_PAGE;
    bool   is_dirty  = false;
    int    pin_count = 0;
};

class BufferManager {
public:
    BufferManager(DiskManager& disk_manager, size_t num_frames);
    ~BufferManager();

    Page* fetch_page(PageId page_id);
    void  unpin_page(PageId page_id, bool is_dirty);
    Page* new_page(PageId& out_page_id);
    void  flush_all();
    void  print_status() const;

    // Estadísticas para el informe
    double hit_rate() const;
    size_t get_total_requests() const { return total_requests_; }
    size_t get_cache_hits()     const { return cache_hits_; }

private:
    DiskManager&                        disk_;
    size_t                              num_frames_;
    std::vector<Page>                   frames_;
    std::vector<FrameDescriptor>        descriptors_;
    std::unordered_map<PageId, FrameId> page_table_;

    // ── LRU ──────────────────────────────────────────────────────
    // frente = más recientemente usado, final = víctima
    std::list<FrameId>                              lru_list_;
    std::unordered_map<FrameId,
        std::list<FrameId>::iterator>               lru_map_;

    // Estadísticas
    size_t total_requests_ = 0;
    size_t cache_hits_     = 0;

    // Encuentra el frame víctima según LRU
    // Devuelve INVALID_PAGE si todos están pinned
    FrameId find_victim();

    void lru_remove(FrameId fid);
    void lru_add_front(FrameId fid);
};
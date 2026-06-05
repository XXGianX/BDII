#include "buffer_manager.h"
#include <stdexcept>
#include <iostream>
#include <iomanip>

BufferManager::BufferManager(DiskManager& disk, size_t num_frames)
    : disk_(disk), num_frames_(num_frames),
      frames_(num_frames), descriptors_(num_frames)
{
    // Todos los frames inician libres → entran a la lista LRU
    for (FrameId i = 0; i < num_frames_; i++) {
        lru_list_.push_back(i);
        lru_map_[i] = std::prev(lru_list_.end());
    }
}

BufferManager::~BufferManager() {
    flush_all();
}

Page* BufferManager::fetch_page(PageId page_id) {
    total_requests_++;

    // ── HIT: página ya está en el buffer ────────────────────────
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        cache_hits_++;
        FrameId fid = it->second;
        descriptors_[fid].pin_count++;

        // Mover al frente de LRU (más recientemente usado)
        lru_remove(fid);
        lru_add_front(fid);
        return &frames_[fid];
    }

    // ── MISS: traer del disco ────────────────────────────────────
    FrameId fid = find_victim();
    if (fid == static_cast<FrameId>(INVALID_PAGE)) {
        throw std::runtime_error("Buffer lleno: todos los frames están pinned");
    }

    FrameDescriptor& desc = descriptors_[fid];

    // Si el frame víctima tiene página dirty → escribir al disco
    if (desc.is_dirty && desc.page_id != INVALID_PAGE) {
        disk_.write_page(desc.page_id, frames_[fid].data_);
        desc.is_dirty = false;
    }

    // Eliminar entrada vieja de la Page Table
    if (desc.page_id != INVALID_PAGE) {
        page_table_.erase(desc.page_id);
    }

    // Leer la nueva página del disco
    disk_.read_page(page_id, frames_[fid].data_);

    desc.page_id   = page_id;
    desc.is_dirty  = false;
    desc.pin_count = 1;
    page_table_[page_id] = fid;

    // Frame pinned → fuera de la lista LRU
    lru_remove(fid);

    return &frames_[fid];
}

void BufferManager::unpin_page(PageId page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return;

    FrameId fid = it->second;
    FrameDescriptor& desc = descriptors_[fid];

    if (desc.pin_count <= 0) return;
    if (is_dirty) desc.is_dirty = true;

    desc.pin_count--;

    // Si nadie la usa → vuelve a la lista LRU al frente
    if (desc.pin_count == 0) {
        lru_add_front(fid);
    }
}

Page* BufferManager::new_page(PageId& out_page_id) {
    out_page_id = disk_.get_num_pages();

    FrameId fid = find_victim();
    if (fid == static_cast<FrameId>(INVALID_PAGE)) {
        throw std::runtime_error("Buffer lleno: no se puede crear página nueva");
    }

    FrameDescriptor& desc = descriptors_[fid];

    if (desc.is_dirty && desc.page_id != INVALID_PAGE) {
        disk_.write_page(desc.page_id, frames_[fid].data_);
        desc.is_dirty = false;
    }
    if (desc.page_id != INVALID_PAGE) {
        page_table_.erase(desc.page_id);
    }

    frames_[fid] = Page();
    frames_[fid].header()->page_id = out_page_id;
    disk_.write_page(out_page_id, frames_[fid].data_);

    desc.page_id   = out_page_id;
    desc.is_dirty  = false;
    desc.pin_count = 1;
    page_table_[out_page_id] = fid;

    lru_remove(fid);

    return &frames_[fid];
}

void BufferManager::flush_all() {
    for (FrameId fid = 0; fid < num_frames_; fid++) {
        FrameDescriptor& desc = descriptors_[fid];
        if (desc.is_dirty && desc.page_id != INVALID_PAGE) {
            disk_.write_page(desc.page_id, frames_[fid].data_);
            desc.is_dirty = false;
        }
    }
}

double BufferManager::hit_rate() const {
    if (total_requests_ == 0) return 0.0;
    return (double)cache_hits_ / total_requests_ * 100.0;
}

void BufferManager::print_status() const {
    std::cout << "\n--- Buffer Pool (" << num_frames_ << " frames) ---\n";
    std::cout << std::left
              << std::setw(8)  << "Frame"
              << std::setw(10) << "PageId"
              << std::setw(10) << "PinCount"
              << std::setw(8)  << "Dirty"
              << "\n" << std::string(36, '-') << "\n";

    for (FrameId fid = 0; fid < num_frames_; fid++) {
        const FrameDescriptor& d = descriptors_[fid];
        std::string pid = (d.page_id == INVALID_PAGE)
                        ? "libre" : std::to_string(d.page_id);
        std::cout << std::left
                  << std::setw(8)  << fid
                  << std::setw(10) << pid
                  << std::setw(10) << d.pin_count
                  << std::setw(8)  << (d.is_dirty ? "si" : "no")
                  << "\n";
    }

    // Mostrar orden LRU (frente = más reciente, final = víctima)
    std::cout << "LRU orden: [";
    for (FrameId fid : lru_list_) std::cout << fid << " ";
    std::cout << "] (frente=reciente, final=victima)\n";

    std::cout << "Page Table: ";
    for (auto& [pid, fid] : page_table_)
        std::cout << "P" << pid << "->F" << fid << " ";
    std::cout << "\nHit rate: " << hit_rate() << "%\n";
    std::cout << "-----------------------------------\n\n";
}

// ── LRU internals ────────────────────────────────────────────────

FrameId BufferManager::find_victim() {
    // Recorrer desde el final (menos usado) hasta el frente
    for (auto rit = lru_list_.rbegin(); rit != lru_list_.rend(); ++rit) {
        FrameId fid = *rit;
        if (descriptors_[fid].pin_count == 0) {
            lru_remove(fid);
            return fid;
        }
    }
    return static_cast<FrameId>(INVALID_PAGE);
}

void BufferManager::lru_remove(FrameId fid) {
    auto it = lru_map_.find(fid);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_map_.erase(it);
    }
}

void BufferManager::lru_add_front(FrameId fid) {
    lru_list_.push_front(fid);
    lru_map_[fid] = lru_list_.begin();
}
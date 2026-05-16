#include "buffer_manager.h"
#include <stdexcept>
#include <iostream>
#include <iomanip>

BufferManager::BufferManager(DiskManager& disk, size_t num_frames)
    : disk_(disk), num_frames_(num_frames),
      frames_(num_frames), descriptors_(num_frames)
{
    // Todos los frames inician vacíos — page_id = INVALID_PAGE
}

BufferManager::~BufferManager() {
    flush_all();
}

Page* BufferManager::fetch_page(PageId page_id) {

    // ── Caso 1: la página YA está en el buffer (HIT) ────────────
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        FrameId fid = it->second;
        descriptors_[fid].pin_count++;
        return &frames_[fid];
    }

    // ── Caso 2: la página NO está en el buffer (MISS) ───────────
    FrameId fid = find_free_frame();
    if (fid == static_cast<FrameId>(INVALID_PAGE)) {
        throw std::runtime_error("Buffer lleno: no hay frames disponibles");
    }

    // Leer la página del disco al frame
    disk_.read_page(page_id, frames_[fid].data_);

    // Actualizar descriptor y Page Table
    descriptors_[fid].page_id   = page_id;
    descriptors_[fid].is_dirty  = false;
    descriptors_[fid].pin_count = 1;
    page_table_[page_id]        = fid;

    return &frames_[fid];
}

void BufferManager::unpin_page(PageId page_id, bool is_dirty) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return; // no estaba en el buffer

    FrameId fid = it->second;

    if (descriptors_[fid].pin_count <= 0) return;

    if (is_dirty) descriptors_[fid].is_dirty = true;

    descriptors_[fid].pin_count--;
}

Page* BufferManager::new_page(PageId& out_page_id) {
    out_page_id = disk_.get_num_pages();

    FrameId fid = find_free_frame();
    if (fid == static_cast<FrameId>(INVALID_PAGE)) {
        throw std::runtime_error("Buffer lleno: no se puede crear página nueva");
    }

    // Inicializar la página nueva
    frames_[fid] = Page();
    frames_[fid].header()->page_id = out_page_id;

    // Escribirla en disco para reservar su posición
    disk_.write_page(out_page_id, frames_[fid].data_);

    descriptors_[fid].page_id   = out_page_id;
    descriptors_[fid].is_dirty  = false;
    descriptors_[fid].pin_count = 1;
    page_table_[out_page_id]    = fid;

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

void BufferManager::print_status() const {
    std::cout << "\n--- Estado del Buffer Pool (" << num_frames_ << " frames) ---\n";
    std::cout << std::left
              << std::setw(8)  << "Frame"
              << std::setw(10) << "PageId"
              << std::setw(10) << "PinCount"
              << std::setw(8)  << "Dirty"
              << "\n";
    std::cout << std::string(36, '-') << "\n";
    for (FrameId fid = 0; fid < num_frames_; fid++) {
        const FrameDescriptor& d = descriptors_[fid];
        std::string pid = (d.page_id == INVALID_PAGE) ? "libre" : std::to_string(d.page_id);
        std::cout << std::left
                  << std::setw(8)  << fid
                  << std::setw(10) << pid
                  << std::setw(10) << d.pin_count
                  << std::setw(8)  << (d.is_dirty ? "si" : "no")
                  << "\n";
    }
    std::cout << "Page Table: ";
    for (auto& [pid, fid] : page_table_) {
        std::cout << "P" << pid << "→F" << fid << " ";
    }
    std::cout << "\n-----------------------------------\n\n";
}

FrameId BufferManager::find_free_frame() {
    // busca el primer frame vacío
    for (FrameId fid = 0; fid < num_frames_; fid++) {
        if (descriptors_[fid].page_id == INVALID_PAGE) {
            return fid;
        }
    }
    return static_cast<FrameId>(INVALID_PAGE);
}
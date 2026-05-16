#pragma once
#include "../common/config.h"
#include "../common/types.h"
#include <cstring>

struct PageHeader {
    PageId   page_id        = INVALID_PAGE;
    uint16_t num_slots      = 0;
    uint16_t free_space_ptr = static_cast<uint16_t>(PAGE_SIZE);
    uint8_t  reserved[8]   = {};
};

struct SlotEntry {
    uint16_t offset = 0;
    uint16_t length = 0;
};

class Page {
public:
    char data_[PAGE_SIZE];

    Page() {
        memset(data_, 0, PAGE_SIZE);
        PageHeader* h     = reinterpret_cast<PageHeader*>(data_);
        h->page_id        = INVALID_PAGE;
        h->num_slots      = 0;
        h->free_space_ptr = static_cast<uint16_t>(PAGE_SIZE);
    }

    PageHeader* header() {
        return reinterpret_cast<PageHeader*>(data_);
    }

    SlotEntry* slot_directory() {
        return reinterpret_cast<SlotEntry*>(data_ + sizeof(PageHeader));
    }

    SlotEntry* get_slot(SlotId sid) {
        return &slot_directory()[sid];
    }

    uint16_t free_space() const {
        auto* h = reinterpret_cast<const PageHeader*>(data_);
        uint16_t used = static_cast<uint16_t>(
            sizeof(PageHeader) + h->num_slots * sizeof(SlotEntry)
        );
        if (h->free_space_ptr < used) return 0;
        return h->free_space_ptr - used;
    }

    char* get_record_ptr(SlotId sid) {
        SlotEntry* s = get_slot(sid);
        if (s->length == 0) return nullptr;
        return data_ + s->offset;
    }
};

#pragma once
#include <cstdint>

using PageId  = uint32_t;  // identificador de página en disco
using SlotId  = uint16_t;  // índice de slot dentro de una página
using FrameId = uint32_t;  // índice de frame en el buffer pool

struct RID {               // Record ID: ubicación física de un registro
    PageId  page_id;
    SlotId  slot_id;
};